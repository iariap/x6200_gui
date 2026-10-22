/*
 * Test nativo del fail-slow de radio_thread sin BASE (issue #219, evidencia F-08
 * del dossier #194).
 *
 * Desde el #244 el mismo binario cubre ademas el camino del audio de recepcion
 * (radio_change_mute / radio_change_vol / radio_vol_apply / re-sync al salir de CW):
 * el valor que llega a x6200_control_rxvol_set() —o sea al registro 14 del BASE— se
 * observa con el stub de tests/native/radio_stubs.c. Ver docs/HALLAZGOS.md 64.
 *
 * Hallazgo: src/radio.c::radio_thread() solo dormia cuando radio_tick() devolvia
 * true (`if (radio_tick()) usleep(15000);`). Cuando devolvia false el hilo giraba
 * tan rapido como permitiera la plataforma: en QEMU_USER sin BASE se midieron
 * ~99.8% de un core host y ~630k read()/s (04-performance del #198). El contrato
 * del #219 es que el sondeo duerma SIEMPRE, sin cambiar la semantica de la radio.
 *
 * Como se observa el lazo real: src/radio.c se compila por inclusion (mismo
 * patron que tests/native/test_db_persistence.c con src/params/db.c), asi que el
 * estatico radio_thread() y su estado privado quedan visibles. radio_tick() no se
 * stubea: se controla su valor de retorno por su unica entrada observable,
 * x6200_flow_read() (stub de tests/native/radio_stubs.c):
 *
 *     x6200_flow_read() == true   -> radio_tick() procesa el paquete y devuelve false
 *     x6200_flow_read() == false  -> radio_tick() corre el watchdog y devuelve true
 *
 * El lazo se corta con un longjmp desde el stub de usleep (o desde el de
 * x6200_flow_read) tras la cantidad pedida de iteraciones: no se usa tiempo real,
 * asi que el resultado es determinista.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <setjmp.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#include "test_framework.h"
#include "radio_stubs.h"

#ifndef RADIO_C_PATH
#error "definir RADIO_C_PATH al src/radio.c stageado (tests/native/run_native_tests.sh)"
#endif

#include RADIO_C_PATH

static x6200_flow_t test_pack;

/* Estado del lazo que viven en radio.c y hay que devolver a cero entre casos
 * (radio_stub_reset() se ocupa de los contadores y del reloj falso). */
static void reset_loop(void) {
    radio_stub_reset();

    now_time  = 0;
    prev_time = 0;
    idle_time = 0;
    state     = RADIO_RX;
    pack      = &test_pack;
    memset(&test_pack, 0, sizeof(test_pack));
}

/* Corre radio_thread() `n` iteraciones completas. El corte normal es el stub de
 * usleep (una vez por iteracion en la version corregida); ademas se arma el corte
 * por lectura para que la version con el defecto (que no duerme) termine igual y
 * el test falle en el CHECK en vez de colgarse. En la corregida: reads == sleeps
 * == n; en la defectuosa: sleeps == 0 y reads == n+1. */
static void run_iterations(int n) {
    int reads0  = (int) radio_stub_flow_reads;
    int sleeps0 = radio_stub_sleep_calls;

    radio_stub_flow_read_limit = reads0 + n + 1;   /* red de seguridad */
    radio_stub_sleep_limit     = sleeps0 + n;      /* corte normal */
    radio_stub_jmp_armed       = true;

    if (setjmp(radio_stub_jmp) == 0) {
        radio_thread(NULL);
    }

    radio_stub_jmp_armed = false;
}

/* --- 1. radio_tick() false: el caso del busy-spin -------------------------- */

/* radio_tick() false 1000 veces (flow_read true, paquete a bordo): antes NO
 * dormia ni una vez; ahora tiene que dormir 1000 veces de exactamente 15000 us. */
static void test_tick_false_duerme_siempre(void) {
    reset_loop();
    radio_stub_flow_ok = true;

    run_iterations(1000);

    CHECK(radio_stub_flow_reads == 1000);
    CHECK(radio_stub_sleep_calls == 1000);
    CHECK(radio_stub_sleep_us_total == 1000ull * 15000ull);
    CHECK(radio_stub_saw_bad_sleep == false);
}

/* --- 2. radio_tick() true: la secuencia no cambia ------------------------- */

/* radio_tick() true 1000 veces (flow_read false, sin paquete): ya dormia antes;
 * la secuencia tiene que ser la MISMA que en el caso 1 (1000 x 15000 us). */
static void test_tick_true_misma_secuencia(void) {
    reset_loop();
    radio_stub_flow_ok = false;

    run_iterations(1000);

    CHECK(radio_stub_flow_reads == 1000);
    CHECK(radio_stub_sleep_calls == 1000);
    CHECK(radio_stub_sleep_us_total == 1000ull * 15000ull);
    CHECK(radio_stub_saw_bad_sleep == false);
}

/* --- 3. Mezcla 1 false / 1 true / 100 false -------------------------------- */

static void test_mezcla(void) {
    static bool pattern[102];

    pattern[0] = true;                /* radio_tick() false (paquete)         */
    pattern[1] = false;               /* radio_tick() true  (sin paquete)     */
    for (int i = 2; i < 102; i++) {
        pattern[i] = true;            /* radio_tick() false                   */
    }

    reset_loop();
    radio_stub_flow_pattern     = pattern;
    radio_stub_flow_pattern_len = 102;

    run_iterations(102);

    CHECK(radio_stub_flow_reads == 102);
    CHECK(radio_stub_sleep_calls == 102);
    CHECK(radio_stub_sleep_us_total == 102ull * 15000ull);
    CHECK(radio_stub_saw_bad_sleep == false);
}

/* --- 4. x6200_control_idle() respeta su periodo ---------------------------- */

/* Con el sleep incondicional el lazo corre a ~67 iteraciones/s (15000 us). El
 * chequeo de IDLE_TIMEOUT (1000 ms) tiene que disparar exactamente cada 67
 * iteraciones: ni de mas ni de menos, o sea el presupuesto de idle no se altera. */
static void test_idle_respeta_timeout(void) {
    reset_loop();
    radio_stub_flow_ok = true;        /* radio_tick() false: el camino congestionado */

    run_iterations(1000);

    CHECK(radio_stub_idle_marks_n == 14);
    CHECK(radio_stub_idle_marks[0] == 68);

    for (int i = 1; i < radio_stub_idle_marks_n; i++) {
        CHECK(radio_stub_idle_marks[i] - radio_stub_idle_marks[i - 1] == 67);
    }

    CHECK(radio_stub_idle_calls == radio_stub_idle_marks_n);
}

/* --- 5. El watchdog del barrido de ROE corre en cada vuelta ---------------- */

/* En RADIO_SWRSCAN el watchdog (dialog_swrscan_poll) tiene que evaluarse en cada
 * iteracion del lazo para poder abortar el barrido por timeout: el sleep nuevo no
 * lo saltea. Es la garantia de que el fail-slow no toca el lazo de seguridad. */
static void test_swrscan_poll_cada_iteracion(void) {
    reset_loop();
    radio_stub_flow_ok = false;       /* radio_tick() true: rama del watchdog */
    state = RADIO_SWRSCAN;

    run_iterations(500);

    CHECK(radio_stub_sleep_calls == 500);
    CHECK(radio_stub_poll_calls == 500);
}

/* --- 6. Recuperacion al volver el paquete ---------------------------------- */

/* Sin paquete durante una ventana (BASE ausente) y despues un paquete: el hilo
 * retoma en la PRIMERA vuelta siguiente (a lo sumo un sleep de 15 ms), procesa el
 * paquete (dsp_samples) y no pierde el estado (sigue en RADIO_RX). No hay retraso
 * arbitrario ni perdida de estado. */
static void test_recupera_al_volver_el_paquete(void) {
    reset_loop();

    radio_stub_flow_ok = false;       /* ventana sin paquete (sin BASE) */
    run_iterations(300);

    CHECK(radio_stub_dsp_calls == 0); /* nada que procesar */
    CHECK(state == RADIO_RX);

    radio_stub_flow_ok = true;        /* vuelve el paquete */
    int reads_before = (int) radio_stub_flow_reads;

    run_iterations(1);

    CHECK(radio_stub_flow_reads == (uint64_t) reads_before + 1);
    CHECK(radio_stub_sleep_calls == 301);
    CHECK(radio_stub_dsp_calls == 1); /* el paquete se proceso de inmediato */
    CHECK(state == RADIO_RX);         /* el estado no se perdio */
}

/* --- Audio de recepcion (#244) -------------------------------------------- */

/* Estos casos llaman a las funciones REALES de src/radio.c (compilado por inclusion) y miran
 * que valor le llega a x6200_control_rxvol_set(), o sea al registro 14 del BASE. El defecto que
 * cubren: el nivel se calculaba con un toggle y cada camino que cambiaba el mute escribia (o no)
 * el registro por su cuenta, asi que la GUI podia quedar diciendo "no estoy muda" con el equipo
 * en 0 — el sintoma del #244 (silencio total sin causa visible).
 *
 * cfg.vol.val y cfg_cur.mode son Subjects de mentira (los valores los da el stub, ver
 * radio_stubs.h), asi que no se dereferencian. */
static void vol_setup(void) {
    static bool mux_init = false;

    /* radio_vol_apply() toma radio_lock(); radio_init() no corre en el test, asi que el mutex se
     * crea aca una vez. */
    if (!mux_init) {
        pthread_mutex_init(&control_mux, NULL);
        mux_init = true;
    }

    radio_stub_reset();

    cfg.vol.val    = (Subject *) 0x1;
    cfg_cur.mode   = (Subject *) 0x2;
    radio_stub_vol = 40;
}

static void test_vol_composicion_de_mutes(void) {
    vol_setup();
    mute = false;

    /* Sin mutes: el nivel guardado va tal cual. */
    radio_vol_apply();
    CHECK(radio_stub_rxvol_calls == 1);
    CHECK(radio_stub_rxvol_last == 40);

    /* Mute del usuario: 0; y al desmutear vuelve el nivel guardado (no el toggle a ciegas). */
    radio_change_mute();
    CHECK(radio_is_muted() == true);
    CHECK(radio_stub_rxvol_last == 0);
    radio_change_mute();
    CHECK(radio_is_muted() == false);
    CHECK(radio_stub_rxvol_last == 40);

    /* Mute por auriculares A2DP: se COMPONE con el del usuario. Con el altavoz muteado por BT,
     * desmutear a mano no puede devolver el audio (antes mandaba 40: el altavoz sonaba con los
     * auriculares puestos). */
    radio_stub_bt_muted = true;
    radio_change_mute();                       /* usuario mutea */
    CHECK(radio_stub_rxvol_last == 0);
    radio_change_mute();                       /* usuario desmutea: sigue en 0 por BT */
    CHECK(radio_is_muted() == false);
    CHECK(radio_stub_rxvol_last == 0);

    /* Al desconectarse los auriculares, la politica de BT re-aplica y vuelve el nivel. */
    radio_stub_bt_muted = false;
    radio_vol_apply();
    CHECK(radio_stub_rxvol_last == 40);
}

static void test_vol_perilla_en_el_tope_no_desincroniza(void) {
    vol_setup();
    radio_stub_vol = 0;
    mute = true;

    radio_vol_apply();
    int writes = radio_stub_rxvol_calls;

    /* Nivel 0 y el usuario baja: el nivel no se mueve (limit), pero el mute se limpia. Antes el
     * registro no se volvia a mandar y la GUI quedaba "no muda" con el equipo en 0. */
    CHECK(radio_change_vol(-1) == 0);
    CHECK(radio_stub_subject_set_calls == 0);      /* el nivel no cambio */
    CHECK(radio_is_muted() == false);
    CHECK(radio_stub_rxvol_calls == writes + 1);   /* pero el registro se re-mando */
    CHECK(radio_stub_rxvol_last == 0);
}

static void test_vol_resync_al_salir_de_cw(void) {
    vol_setup();

    radio_stub_vol  = 40;
    radio_stub_mode = x6200_mode_usb;
    on_cur_mode_change_vol(cfg_cur.mode, NULL);            /* arranca en USB: no escribe */
    CHECK(radio_stub_rxvol_calls == 0);

    radio_stub_mode = x6200_mode_cw;
    on_cur_mode_change_vol(cfg_cur.mode, NULL);            /* entra a CW: no escribe */
    CHECK(radio_stub_rxvol_calls == 0);

    radio_stub_mode = x6200_mode_lsb;
    on_cur_mode_change_vol(cfg_cur.mode, NULL);            /* SALE de CW: re-manda el nivel */
    CHECK(radio_stub_rxvol_calls == 1);
    CHECK(radio_stub_rxvol_last == 40);

    /* Con el usuario muteado el re-sync no puede devolver el audio. */
    mute = true;
    radio_stub_mode = x6200_mode_cw;
    on_cur_mode_change_vol(cfg_cur.mode, NULL);
    radio_stub_mode = x6200_mode_usb;
    on_cur_mode_change_vol(cfg_cur.mode, NULL);
    CHECK(radio_stub_rxvol_calls == 2);
    CHECK(radio_stub_rxvol_last == 0);
}

void test_radio_thread(void) {
    test_tick_false_duerme_siempre();
    test_tick_true_misma_secuencia();
    test_mezcla();
    test_idle_respeta_timeout();
    test_swrscan_poll_cada_iteracion();
    test_recupera_al_volver_el_paquete();

    test_vol_composicion_de_mutes();
    test_vol_perilla_en_el_tope_no_desincroniza();
    test_vol_resync_al_salir_de_cw();
}

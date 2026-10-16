/*
 * Native test for src/dialog_swrscan.c (issue #85: "Cerrar la app con el barrido
 * corriendo reinicia la GUI", SIGSEGV fallo=0x4).
 *
 * The defect: the dialog's destructor stopped the sweep through its own button
 * callback with a NULL item -- `destruct_cb(): if (run) dialog_swrscan_run_cb(NULL)`
 * -- and that callback's first line is `button_update_label(item, "Run")`, which
 * dereferences `item->type` (offset 4 of button_item_t). The radio died with
 * `CRASHLOG sig=11 fallo=0x4` on the close path (scheduler_work ->
 * main_screen_close_app -> dialog_destruct -> destruct_cb -> apply_cmd); it was
 * reproduced on the bench with the shipping binary (md5 d9ccb7d4c224) by opening
 * SWRScan, pressing Run and closing: `pc` resolved inside button_update_label and
 * `lr` inside destruct_cb.
 *
 * The invariants this test pins: closing the dialog with the sweep running stops
 * the sweep WITHOUT handing a NULL item to a button callback, and the teardown is
 * idempotent -- destruct_cb plus the LV_EVENT_DELETE that follows stop it once,
 * and LV_EVENT_DELETE alone (without destruct_cb) also stops it.
 *
 * The REAL src/dialog_swrscan.c is staged next to minimal stubs
 * (tests/native/stubs_swrscan/), the same pattern as the clock/backlight/
 * digital-modes tests. The button_update_label stub records the NULL case instead
 * of crashing (the real one dereferences it), so a regression fails an assertion.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_framework.h"

#include "dialog_swrscan.h"
#include "buttons.h"

/* Provided by tests/native/swrscan_stubs.c. */
void swrscan_stub_reset(void);
int  swrscan_stub_null_label(void);
int  swrscan_stub_started(void);
int  swrscan_stub_stopped(void);
void swrscan_stub_set_fg_freq(int32_t v);
void swrscan_stub_set_span(int32_t v);
void swrscan_stub_fire_delete(void);
int  swrscan_stub_delete_cb_registered(void);
int32_t swrscan_stub_freq(int i);
int  swrscan_stub_freq_count(void);

static button_item_t *run_button(void) {
    return dialog_swrscan->btn_page->items[0];
}

void test_swrscan(void) {
    swrscan_stub_reset();
    swrscan_stub_set_fg_freq(7018300);      /* 40 m CW, the field case of the issue */
    swrscan_stub_set_span(50000);

    dialog_swrscan->construct_cb(NULL);
    CHECK(swrscan_stub_delete_cb_registered());     /* the LV_EVENT_DELETE safety net */

    button_item_t *run = run_button();

    /* --- ciclo 1: abrir, arrancar el barrido, cerrar (el camino del `off`) --- */
    run->press(run);
    CHECK(swrscan_stub_started() == 1);
    CHECK(swrscan_stub_null_label() == 0);

    dialog_swrscan->destruct_cb();                  /* lo que llama dialog_destruct */
    CHECK(swrscan_stub_null_label() == 0);          /* <- falla antes del fix */
    CHECK(swrscan_stub_stopped() == 1);

    /* Idempotente: el LV_EVENT_DELETE que sigue no lo apaga dos veces. */
    swrscan_stub_fire_delete();
    CHECK(swrscan_stub_stopped() == 1);
    CHECK(swrscan_stub_null_label() == 0);

    /* --- ciclo 2: reabrir, arrancar, cerrar otra vez --- */
    dialog_swrscan->construct_cb(NULL);
    run->press(run);
    CHECK(swrscan_stub_started() == 2);
    dialog_swrscan->destruct_cb();
    CHECK(swrscan_stub_null_label() == 0);
    CHECK(swrscan_stub_stopped() == 2);

    /* --- LV_EVENT_DELETE solo (sin destruct_cb) tambien apaga el barrido --- */
    dialog_swrscan->construct_cb(NULL);
    run->press(run);
    CHECK(swrscan_stub_started() == 3);
    swrscan_stub_fire_delete();
    CHECK(swrscan_stub_stopped() == 3);
    CHECK(swrscan_stub_null_label() == 0);
}

/*
 * Ampliaciones del owner sobre el #113 (2026-10-16): el barrido se adapta al span ("aumentar la
 * frecuencia de barrido como lo hace el nanovna") y TRANSMITE al piso de potencia del equipo
 * ("la potencia de transmision durante el barrido debe ser la minima posible").
 *
 * El barrido avanza un punto por paquete del flow de la radio, que tarda ~35 ms (flow.h:
 * "Usually a packet arrives every 35ms"), asi que la duracion es puntos x dwell; el dwell no es
 * un parametro de la GUI (aether_control solo expone x6200_control_swrscan_set on/off, sin
 * cadencia). El conteo sale del span (dialog_swrscan_points_for_span) y el presupuesto de
 * duracion lo fija el codigo (SWRSCAN_TX_BUDGET_MS; subirlo no compila, ver el _Static_assert de
 * dialog_swrscan.c).
 *
 * Lo que se fija aca: la tabla span -> puntos -> paso exacto -> duracion, los extremos en los
 * limites del span, la grilla determinista, el piso de potencia y que el dialogo siga apagando
 * el barrido al cerrarse.
 */
void test_swrscan_sweep(void) {
    const uint32_t center = 7018300;    /* 40 m CW */

    /* La tabla del modo automatico: el span manda el conteo y el paso parte el span exacto. */
    static const struct {
        int32_t span_hz;
        int32_t points;
        int32_t step_hz;
    } table[] = {
        {  50000, 41, 1250 },
        { 100000, 51, 2000 },
        { 200000, 51, 4000 },
        { 500000, 51, 10000 },
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        int32_t span_hz = table[i].span_hz;
        int32_t points  = dialog_swrscan_points_for_span(span_hz);

        CHECK(points == table[i].points);
        CHECK(span_hz % (points - 1) == 0);                        /* el paso parte el span exacto */
        CHECK(span_hz / (points - 1) == table[i].step_hz);
        CHECK(span_hz / (points - 1) >= SWRSCAN_STEP_TARGET_HZ);   /* nunca mas fino que el objetivo */
        CHECK(points * SWRSCAN_DWELL_MS <= SWRSCAN_TX_BUDGET_MS);  /* y entra en el presupuesto */

        /* Borde a borde: index 0 es el borde bajo del span y index puntos-1 el borde alto. */
        CHECK(dialog_swrscan_sweep_freq(center, span_hz, 0) == center - (uint32_t) span_hz / 2);
        CHECK(dialog_swrscan_sweep_freq(center, span_hz, points - 1) == center + (uint32_t) span_hz / 2);
    }

    /* El piso del conteo: 25 kHz / 1250 Hz = 20 intervalos, por debajo de SWRSCAN_MIN_POINTS-1,
     * asi que manda el piso (spans chicos densos) y el paso sigue partiendo el span exacto. */
    CHECK(dialog_swrscan_points_for_span(25000) == SWRSCAN_MIN_POINTS);
    CHECK(25000 % (dialog_swrscan_points_for_span(25000) - 1) == 0);
    CHECK(dialog_swrscan_points_for_span(25000) * SWRSCAN_DWELL_MS <= SWRSCAN_TX_BUDGET_MS);

    /* Span sin divisores en el rango (11111 = 41 x 271): manda la exactitud -- 2 puntos, paso =
     * span -- en vez de un paso con resto. */
    CHECK(dialog_swrscan_points_for_span(11111) == 2);
    CHECK(11111 % (dialog_swrscan_points_for_span(11111) - 1) == 0);
    CHECK(dialog_swrscan_sweep_freq(center, 11111, 1) - dialog_swrscan_sweep_freq(center, 11111, 0) == 11111);

    /* El barrido real del dialogo con el span del caso de campo (50 kHz -> 41 puntos). */
    const int32_t span   = 50000;
    const int     points = dialog_swrscan_points_for_span(span);

    CHECK(points == 41);

    swrscan_stub_reset();
    swrscan_stub_set_fg_freq((int32_t) center);
    swrscan_stub_set_span(span);

    dialog_swrscan->construct_cb(NULL);
    button_item_t *run = run_button();

    run->press(run);                                        /* Run: do_init + set_freq(borde bajo) */
    CHECK(swrscan_stub_freq(0) == (int32_t) center - span / 2);

    for (int k = 1; k <= points; k++) {
        dialog_swrscan_update(1.5f);                        /* un paquete por punto */
    }

    CHECK(swrscan_stub_freq_count() == points + 1);         /* los puntos + la vuelta al inicio */

    for (int k = 1; k < points; k++) {                      /* paso uniforme = span / (puntos-1) */
        CHECK(swrscan_stub_freq(k) - swrscan_stub_freq(k - 1) == span / (points - 1));
    }

    /* El ultimo punto medido es el borde ALTO del span (borde a borde: el paso parte el span
     * exacto) y despues el barrido vuelve al borde bajo. */
    CHECK(swrscan_stub_freq(points - 1) == (int32_t) center + span / 2);
    CHECK(swrscan_stub_freq(points) == (int32_t) center - span / 2);

    int32_t first[SWRSCAN_MAX_POINTS + 1];

    for (int k = 0; k <= points; k++) {
        first[k] = swrscan_stub_freq(k);
    }

    /* Determinismo: el mismo barrido da la misma grilla. */
    run->press(run);                                        /* Stop */
    swrscan_stub_reset();
    run->press(run);                                        /* Run otra vez */

    for (int k = 1; k <= points; k++) {
        dialog_swrscan_update(1.5f);
    }

    CHECK(swrscan_stub_freq_count() == points + 1);

    int32_t again[SWRSCAN_MAX_POINTS + 1];

    for (int k = 0; k <= points; k++) {
        again[k] = swrscan_stub_freq(k);
    }

    CHECK(memcmp(first, again, (size_t) (points + 1) * sizeof(first[0])) == 0);

    run->press(run);                                        /* Stop: deja el dialogo quieto */

    /* La potencia del barrido es el piso del control del equipo (issue #113: "la minima
     * posible"): 0.1 W = 1 decima de watt, el minimo que aceptan el control del panel
     * (src/vol.c, VOL_PWR: LV_MAX(0.1f, ...)) y el control remoto (src/remote_server.cpp,
     * {"pwr", CMD_PWR, 1, 100} en decimas). El barrido ponia 3 W fijos (radio_start_swrscan). */
    CHECK(SWRSCAN_TX_POWER_DW == SWRSCAN_PWR_FLOOR_DW);
    CHECK(SWRSCAN_TX_POWER_DW >= 1);
    CHECK(SWRSCAN_TX_POWER_DW <= SWRSCAN_PWR_CEILING_DW);
    CHECK(SWRSCAN_TX_POWER_W == 0.1f);
    CHECK(SWRSCAN_TX_POWER_W < 3.0f);

    /* El presupuesto de duracion es exactamente el barrido mas largo, dentro de los ~2 s que fijo
     * el owner; y los 101 puntos del NanoVNA no entran (101 x 35 = 3535 ms). */
    CHECK(SWRSCAN_TX_BUDGET_MS == SWRSCAN_MAX_POINTS * SWRSCAN_DWELL_MS);
    CHECK(SWRSCAN_TX_BUDGET_MS <= 2000);
    CHECK(101 * SWRSCAN_DWELL_MS > SWRSCAN_TX_BUDGET_MS);

    printf("barrido #113 automatico por span (paso objetivo %d Hz, %d..%d puntos):\n",
           SWRSCAN_STEP_TARGET_HZ, SWRSCAN_MIN_POINTS, SWRSCAN_MAX_POINTS);

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        int32_t points_i = dialog_swrscan_points_for_span(table[i].span_hz);

        printf("  span %6d Hz -> %2d puntos, paso %5d Hz, %4d ms de barrido (presupuesto %d ms)\n",
               table[i].span_hz, points_i, table[i].step_hz, points_i * SWRSCAN_DWELL_MS,
               SWRSCAN_TX_BUDGET_MS);
    }

    printf("  potencia del barrido: %.1f W (%d decimas: el piso del control del equipo, antes 3.0 W)\n",
           SWRSCAN_TX_POWER_W, SWRSCAN_TX_POWER_DW);
}

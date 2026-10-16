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
 * Ampliacion del owner sobre el #113: "aumentar la frecuencia de barrido como lo hace el
 * nanovna". El barrido TRANSMITE mientras mide (radio_start_swrscan pone 3 W en LSB) y
 * avanza un punto por paquete del flow de la radio, que tarda ~35 ms (flow.h: "Usually a
 * packet arrives every 35ms"): el tiempo al aire es puntos x dwell. El dwell no es un
 * parametro de la GUI (aether_control solo expone x6200_control_swrscan_set on/off, sin
 * cadencia), asi que mas puntos = mas tiempo de TX y el techo lo fija el codigo
 * (SWRSCAN_TX_BUDGET_MS; subirlo no compila, ver el _Static_assert de dialog_swrscan.c).
 *
 * Lo que se fija aca: el paso parte el span exacto, el barrido va de punta a punta del span,
 * la grilla es determinista y el tiempo estimado no crece.
 */
void test_swrscan_sweep(void) {
    const int32_t center = 7018300;
    const int32_t span   = 50000;

    swrscan_stub_reset();
    swrscan_stub_set_fg_freq(center);
    swrscan_stub_set_span(span);

    dialog_swrscan->construct_cb(NULL);
    button_item_t *run = run_button();

    run->press(run);                                        /* Run: do_init + set_freq(start) */
    CHECK(swrscan_stub_freq(0) == center - span / 2);       /* el borde bajo del span */

    for (int k = 1; k <= SWRSCAN_POINTS; k++) {
        dialog_swrscan_update(1.5f);                        /* un paquete por punto */
    }

    CHECK(swrscan_stub_freq_count() == SWRSCAN_POINTS + 1); /* los puntos + la vuelta al inicio */

    for (int k = 1; k < SWRSCAN_POINTS; k++) {              /* paso uniforme = span / puntos */
        CHECK(swrscan_stub_freq(k) - swrscan_stub_freq(k - 1) == span / SWRSCAN_POINTS);
    }

    /* El ultimo punto medido mas un paso es el borde alto del span (el barrido va de punta a
     * punta: el paso parte el span exacto), y despues vuelve al borde bajo. */
    CHECK(swrscan_stub_freq(SWRSCAN_POINTS - 1) + span / SWRSCAN_POINTS == center + span / 2);
    CHECK(swrscan_stub_freq(SWRSCAN_POINTS) == center - span / 2);

    int32_t first[SWRSCAN_POINTS + 1];

    for (int k = 0; k <= SWRSCAN_POINTS; k++) {
        first[k] = swrscan_stub_freq(k);
    }

    /* Determinismo: el mismo barrido da la misma grilla. */
    run->press(run);                                        /* Stop */
    swrscan_stub_reset();
    run->press(run);                                        /* Run otra vez */

    for (int k = 1; k <= SWRSCAN_POINTS; k++) {
        dialog_swrscan_update(1.5f);
    }

    CHECK(swrscan_stub_freq_count() == SWRSCAN_POINTS + 1);

    int32_t again[SWRSCAN_POINTS + 1];

    for (int k = 0; k <= SWRSCAN_POINTS; k++) {
        again[k] = swrscan_stub_freq(k);
    }

    CHECK(memcmp(first, again, sizeof(first)) == 0);

    run->press(run);                                        /* Stop: deja el dialogo quieto */

    /* El paso parte el span exacto en los 4 spans del dialogo. */
    static const int32_t spans[] = {50000, 100000, 200000, 500000};

    for (size_t i = 0; i < sizeof(spans) / sizeof(spans[0]); i++) {
        CHECK(spans[i] % SWRSCAN_POINTS == 0);
    }

    /* El tiempo estimado del barrido no crece: puntos x dwell <= el techo (1750 ms). */
    CHECK(SWRSCAN_POINTS * SWRSCAN_DWELL_MS <= SWRSCAN_TX_BUDGET_MS);

    /* Y el techo no da para los 101 puntos del NanoVNA: 101 x 35 = 3535 ms. */
    CHECK(101 * SWRSCAN_DWELL_MS > SWRSCAN_TX_BUDGET_MS);

    printf("barrido #113: %d puntos x %d ms = %d ms de TX (techo %d ms); 101 puntos costarian "
           "%d ms; span 50 kHz -> paso %d Hz\n",
           SWRSCAN_POINTS, SWRSCAN_DWELL_MS, SWRSCAN_POINTS * SWRSCAN_DWELL_MS,
           SWRSCAN_TX_BUDGET_MS, 101 * SWRSCAN_DWELL_MS, span / SWRSCAN_POINTS);
}

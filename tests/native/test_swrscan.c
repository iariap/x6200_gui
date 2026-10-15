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

#include <stdint.h>

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

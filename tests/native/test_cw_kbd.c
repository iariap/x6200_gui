/*
 * Native test: the CW keyboard dialog does not repaint while idle (#217).
 *
 * The defect (F-07, performance #198): src/dialog_cw_kbd.c::timer_cb() runs every
 * 100 ms and update_view() rewrote log_lbl, head, head_right and state_lbl (and
 * the state chip's two styles) even when nothing changed, so LVGL invalidated the
 * labels four/six times per tick with the dialog at rest. It is not a battery
 * figure (the issue says so), it is invalidations: the fix must not change the
 * keyer, the decoder, the queue or a single visible pixel.
 *
 * The REAL src/dialog_cw_kbd.c runs against tests/native/stubs_cwkbd/ and the
 * counters in tests/native/cwkbd_stubs.c. The contract:
 *   - 100 identical ticks with no stimulus: ZERO label/style writes.
 *   - every real change (mode, ON AIR, sent cursor, WPM, keyboard, theme,
 *     history/queue) still writes, and the text is byte-identical to the base.
 *   - the keyer and the decoder behave exactly as before.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

#include "test_framework.h"

#include "dialog_cw_kbd.h"
#include "cwkbd_stubs.h"
#include "theme.h"
#include "cw_encoder.h"
#include "cfg/cfg_api.h"

#define CW_MODE       x6100_mode_cw
#define NOT_CW_MODE   2            /* USB: anything the dialog reads as "not CW" */
#define TICKS_100     100

/* Label indices in creation order (construct_cb). */
#define LBL_HEAD   0
#define LBL_RIGHT  1
#define LBL_LOG    2
#define LBL_STATE  3
#define LBL_HINT   4

static lv_obj_t *root;

static void build(void) {
    cwkbd_stub_reset();
    root = lv_obj_create(NULL);
    dialog_cw_kbd->construct_cb(root);
}

/* The exact log string the dialog builds (the recolor format of update_view). */
static void expect_log(char *out, size_t size, const char *history, const char *sending, int sent, const char *queue) {
    snprintf(out, size, "#%06x %s##%06x %.*s##%06x %s# #%06x %s#", theme_hex(TC_text_dim), history,
             theme_hex(TC_text), sent, sending, theme_hex(TC_accent), sending + sent, theme_hex(TC_text_faint),
             queue);
}

static void snapshot(char out[LBL_HINT + 1][2048]) {
    for (int i = 0; i <= LBL_HINT; i++) {
        snprintf(out[i], 2048, "%s", cwkbd_stub_label_text(i));
    }
}

/* --- the core: no redraw at rest ------------------------------------------ */

void test_cw_kbd_idle_no_repaint(void) {
    build();

    CHECK(cwkbd_stub_label_count() == LBL_HINT + 1);
    CHECK(strcmp(cwkbd_stub_label_text(LBL_STATE), "READY") == 0);
    CHECK(strcmp(cwkbd_stub_label_text(LBL_RIGHT), "no keyboard: MFK press") == 0);

    char before[LBL_HINT + 1][2048];

    snapshot(before);

    cwkbd_stub_counters_reset();
    for (int i = 0; i < TICKS_100; i++) {
        cwkbd_stub_timer_tick();
    }

    printf("  [medicion] reposo, %d ticks identicos: label_writes=%d style_writes=%d\n", TICKS_100,
           cwkbd_stub_label_writes(), cwkbd_stub_style_writes());

    CHECK(cwkbd_stub_label_writes() == 0);
    CHECK(cwkbd_stub_style_writes() == 0);

    for (int i = 0; i <= LBL_HINT; i++) {
        CHECK(strcmp(before[i], cwkbd_stub_label_text(i)) == 0);   /* 0 cambios visuales */
    }
}

/* --- reactive states ------------------------------------------------------ */

void test_cw_kbd_reactive_state(void) {
    build();
    cwkbd_stub_counters_reset();

    /* mode leaves CW: the chip must say NOT CW and repaint (styles included). */
    cwkbd_stub_set_mode(NOT_CW_MODE);
    cwkbd_stub_timer_tick();
    CHECK(strcmp(cwkbd_stub_label_text(LBL_STATE), "NOT CW") == 0);
    CHECK(cwkbd_stub_label_writes() == 1);
    CHECK(cwkbd_stub_style_writes() == 2);

    /* the same state again: no further writes. */
    cwkbd_stub_counters_reset();
    for (int i = 0; i < 10; i++) {
        cwkbd_stub_timer_tick();
    }
    CHECK(cwkbd_stub_label_writes() == 0);
    CHECK(cwkbd_stub_style_writes() == 0);

    /* back to CW: READY again. */
    cwkbd_stub_counters_reset();
    cwkbd_stub_set_mode(CW_MODE);
    cwkbd_stub_timer_tick();
    CHECK(strcmp(cwkbd_stub_label_text(LBL_STATE), "READY") == 0);
    CHECK(cwkbd_stub_label_writes() == 1);

    /* transmitting: ON AIR, with its own colours. */
    cwkbd_stub_counters_reset();
    cwkbd_stub_set_encoder(CW_ENCODER_SEND, 0);
    cwkbd_stub_timer_tick();
    CHECK(strcmp(cwkbd_stub_label_text(LBL_STATE), "ON AIR") == 0);
    CHECK(cwkbd_stub_label_writes() == 1);
    CHECK(cwkbd_stub_style_writes() == 2);

    /* still ON AIR: nothing. */
    cwkbd_stub_counters_reset();
    for (int i = 0; i < 10; i++) {
        cwkbd_stub_timer_tick();
    }
    CHECK(cwkbd_stub_label_writes() == 0);
    CHECK(cwkbd_stub_style_writes() == 0);
}

/* --- WPM and keyboard lines ----------------------------------------------- */

void test_cw_kbd_header_reacts(void) {
    build();
    cwkbd_stub_counters_reset();

    cwkbd_stub_set_wpm(35);
    cwkbd_stub_timer_tick();
    CHECK(cwkbd_stub_label_writes() == 1);                       /* only the header */
    CHECK(strstr(cwkbd_stub_label_text(LBL_HEAD), "35 WPM") != NULL);
    CHECK(cwkbd_stub_style_writes() == 0);

    cwkbd_stub_counters_reset();
    cwkbd_stub_set_keyboard_ready(true);
    cwkbd_stub_timer_tick();
    CHECK(cwkbd_stub_label_writes() == 1);                       /* only the keyboard line */
    CHECK(strcmp(cwkbd_stub_label_text(LBL_RIGHT), "keyboard ready") == 0);

    cwkbd_stub_counters_reset();
    for (int i = 0; i < 10; i++) {
        cwkbd_stub_timer_tick();
    }
    CHECK(cwkbd_stub_label_writes() == 0);
}

/* --- a live theme change must not leave stale colours ---------------------- */

void test_cw_kbd_theme_change_repaints(void) {
    build();
    cwkbd_stub_counters_reset();

    char before[2048];

    snprintf(before, sizeof(before), "%s", cwkbd_stub_label_text(LBL_LOG));

    cwkbd_stub_set_theme_gen(3);
    cwkbd_stub_timer_tick();

    printf("  [medicion] cambio de tema: label_writes=%d style_writes=%d\n", cwkbd_stub_label_writes(),
           cwkbd_stub_style_writes());

    CHECK(cwkbd_stub_label_writes() >= 3);        /* log + head + state chip text */
    CHECK(cwkbd_stub_style_writes() >= 2);        /* state chip colours */
    CHECK(strcmp(before, cwkbd_stub_label_text(LBL_LOG)) != 0);

    /* the new theme is now cached: idle again. */
    cwkbd_stub_counters_reset();
    for (int i = 0; i < 10; i++) {
        cwkbd_stub_timer_tick();
    }
    CHECK(cwkbd_stub_label_writes() == 0);
    CHECK(cwkbd_stub_style_writes() == 0);
}

/* --- closing and reopening must not reuse a stale cache -------------------- */

void test_cw_kbd_reopen_resets_cache(void) {
    build();
    cwkbd_stub_counters_reset();
    cwkbd_stub_timer_tick();
    CHECK(cwkbd_stub_label_writes() == 0);        /* warm */

    dialog_cw_kbd->destruct_cb();
    CHECK(cwkbd_stub_timer_active() == 0);

    /* a different theme while the dialog is closed: the reopen has to repaint. */
    cwkbd_stub_set_theme_gen(7);
    cwkbd_stub_counters_reset();
    dialog_cw_kbd->construct_cb(root);

    CHECK(cwkbd_stub_timer_active() == 1);
    CHECK(cwkbd_stub_label_writes() >= 4);        /* log + head + right + state */
    CHECK(cwkbd_stub_style_writes() >= 2);

    cwkbd_stub_counters_reset();
    for (int i = 0; i < 10; i++) {
        cwkbd_stub_timer_tick();
    }
    CHECK(cwkbd_stub_label_writes() == 0);
    CHECK(cwkbd_stub_style_writes() == 0);
}

/* --- the exact rendered log (0 visual changes) ----------------------------- */

void test_cw_kbd_log_text_unchanged(void) {
    build();
    cwkbd_stub_set_mode(CW_MODE);
    cwkbd_stub_set_encoder(CW_ENCODER_IDLE, 0);
    cwkbd_stub_counters_reset();

    /* enqueue a line: it goes on the air at once and the log shows it. */
    dialog_cw_kbd_remote("cw_send", 0, "abc");

    char exp[2048];

    cwkbd_stub_set_encoder(CW_ENCODER_SEND, 2);
    cwkbd_stub_timer_tick();
    expect_log(exp, sizeof(exp), "", "ABC ", 2, "");
    CHECK(strcmp(cwkbd_stub_label_text(LBL_LOG), exp) == 0);

    /* the keyer finishes the line: it moves to the history. */
    cwkbd_stub_set_encoder(CW_ENCODER_IDLE, 0);
    cwkbd_stub_timer_tick();
    expect_log(exp, sizeof(exp), "ABC ", "", 0, "");
    CHECK(strcmp(cwkbd_stub_label_text(LBL_LOG), exp) == 0);

    /* a queued line while one is being keyed: sent dim, keyed white, queued faint. */
    cwkbd_stub_set_encoder(CW_ENCODER_IDLE, 0);
    dialog_cw_kbd_remote("cw_send", 0, "de");
    dialog_cw_kbd_remote("cw_send", 0, "fg");
    cwkbd_stub_set_encoder(CW_ENCODER_SEND, 0);
    cwkbd_stub_timer_tick();
    expect_log(exp, sizeof(exp), "ABC ", "DE ", 0, "FG ");
    CHECK(strcmp(cwkbd_stub_label_text(LBL_LOG), exp) == 0);
}

/* --- the keyer and the decoder are untouched ------------------------------- */

void test_cw_kbd_keyer_semantics(void) {
    build();

    /* outside CW the line is refused: no queue, no keying. */
    cwkbd_stub_set_mode(NOT_CW_MODE);
    cwkbd_stub_set_encoder(CW_ENCODER_IDLE, 0);
    cwkbd_stub_counters_reset();
    dialog_cw_kbd_remote("cw_send", 0, "x");
    CHECK(cwkbd_stub_encoder_sends() == 0);

    /* in CW: upper case, '#' dropped, one trailing space, keyed exactly once. */
    cwkbd_stub_set_mode(CW_MODE);
    dialog_cw_kbd_remote("cw_send", 0, "hello#world");
    CHECK(cwkbd_stub_encoder_sends() == 1);
    CHECK(strcmp(cwkbd_stub_last_sent(), "HELLOWORLD ") == 0);
    CHECK(cwkbd_stub_remote_events() == 1);

    /* the render loop never keys on its own. */
    for (int i = 0; i < TICKS_100; i++) {
        cwkbd_stub_timer_tick();
    }
    CHECK(cwkbd_stub_encoder_sends() == 1);

    /* stop clears the queue and stops the keyer. */
    cwkbd_stub_set_encoder(CW_ENCODER_SEND, 3);
    dialog_cw_kbd_remote("cw_stop", 0, NULL);
    CHECK(cwkbd_stub_encoder_stops() == 1);

    /* the web remote's decoder switch reaches cfg.cw_decoder unchanged. */
    dialog_cw_kbd_remote("cw_dec", 1, NULL);
    CHECK(cwkbd_stub_cw_decoder() == 1);

    /* the remote state object keeps reporting the keyer's own values. */
    char state[256];

    cwkbd_stub_set_mode(CW_MODE);
    cwkbd_stub_set_encoder(CW_ENCODER_SEND, 4);
    dialog_cw_kbd_remote("cw_send", 0, "ok");
    dialog_cw_kbd_remote_state(state, sizeof(state));
    CHECK(strstr(state, "\"cw\":1") != NULL);
    CHECK(strstr(state, "\"dec\":1") != NULL);
}

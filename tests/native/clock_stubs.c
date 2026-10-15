/*
 * Stub implementations for the native clock test (tests/native/test_clock.c).
 *
 * They stand in for the small set of LVGL/firmware symbols src/clock.c calls,
 * and expose just enough state for the test to drive the CLOCK_POWER branch of
 * the static show_time(): the label text and the timer callbacks clock_init
 * registers.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "styles.h"
#include "radio.h"
#include "util.h"
#include "voice.h"
#include "msg.h"
#include "params/params.h"

params_t params;

lv_style_t clock_style;
lv_font_t sony_30;
lv_font_t sony_38;

/* src/fonts/font_select.c is not compiled here: the clock test only needs the
 * identity (the ROM's segmented digit families are host-side data). */
const lv_font_t *font_digit(const lv_font_t *stock_slot) {
    return stock_slot;
}

void fonts_set_digit_family(uint8_t family) {
    (void) family;
}

static lv_obj_t  label;
static lv_timer_t timer;
static char       label_text[64];

/* clock_init() registers the clock callback first (show_time, 500 ms) and the
 * time-sync callback second; both are kept so the test can run them. */
#define MAX_TIMERS 4
static void (*timer_cbs[MAX_TIMERS])(lv_timer_t *);
static int    timer_count;

static uint64_t      now_ms = 1000;
static radio_state_t radio_state = RADIO_RX;

lv_obj_t * lv_label_create(lv_obj_t * parent) {
    (void) parent;
    return &label;
}

void lv_obj_add_style(lv_obj_t * obj, const lv_style_t * style, uint32_t selector) {
    (void) obj;
    (void) style;
    (void) selector;
}

void lv_obj_set_style_text_font(lv_obj_t * obj, const lv_font_t * font, uint32_t selector) {
    (void) obj;
    (void) font;
    (void) selector;
}

void lv_obj_set_style_pad_ver(lv_obj_t * obj, lv_coord_t value, uint32_t selector) {
    (void) obj;
    (void) value;
    (void) selector;
}

void lv_obj_set_style_text_align(lv_obj_t * obj, lv_text_align_t align, uint32_t selector) {
    (void) obj;
    (void) align;
    (void) selector;
}

void lv_label_set_text(lv_obj_t * obj, const char * text) {
    (void) obj;
    snprintf(label_text, sizeof(label_text), "%s", text);
}

lv_timer_t * lv_timer_create(void (*timer_cb)(lv_timer_t *), uint32_t period, void * user_data) {
    (void) period;
    (void) user_data;
    if (timer_count < MAX_TIMERS) {
        timer_cbs[timer_count++] = timer_cb;
    }
    return &timer;
}

uint64_t get_time(void) {
    return now_ms;
}

radio_state_t radio_get_state(void) {
    return radio_state;
}

void voice_sure(void) {}

void voice_say_float(const char * prompt, float x) {
    (void) prompt;
    (void) x;
}

void msg_schedule_text_fmt(const char * fmt, ...) {
    (void) fmt;
}

void params_lock(void) {}

void params_unlock(bool * dirty) {
    if (dirty) {
        *dirty = true;
    }
}

/* ---- accessors used by the test ---- */

const char * clock_stub_last_text(void) {
    return label_text;
}

void clock_stub_run_timers(void) {
    for (int i = 0; i < timer_count; i++) {
        timer_cbs[i](&timer);
    }
}

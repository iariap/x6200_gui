/*
 * Stub implementations for the native clock test (tests/native/test_clock.c).
 *
 * They stand in for the small set of LVGL/firmware symbols src/clock.c calls,
 * and expose just enough state for the test to drive the CLOCK_POWER branch of
 * the static show_time(): the label texts and the timer callbacks clock_init
 * registers.
 *
 * clock_init() creates up to two labels: the "8 fantasma" shadow (first, drawn
 * behind) and the value (last) when `font_digit_active()`. Each one is a
 * different object here, so each keeps its own text and the test can check both
 * (the shadow's text is what pins the ghost layer).
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
 * identity (the ROM's segmented digit families are host-side data). The active
 * flag is true, which is the case this test covers: the clock panels drawn with
 * a segmented family, the only case that has an "8 fantasma" layer. */
const lv_font_t *font_digit(const lv_font_t *stock_slot) {
    return stock_slot;
}

bool font_digit_active(void) {
    return true;
}

void fonts_set_digit_family(uint8_t family) {
    (void) family;
}

/* clock_init() creates the shadow first and the value second: the test reads the
 * texts by creation order (`clock_stub_shadow_text` / `clock_stub_last_text`),
 * which is also what pins the z-order (the ghost has to be the lower one). */
#define MAX_LABEL_TEXT 64
#define MAX_LABELS 4
static lv_obj_t   labels[MAX_LABELS];
static char       label_text[MAX_LABELS][MAX_LABEL_TEXT];
static int        label_count;

static lv_timer_t timer;

/* clock_init() registers the clock callback first (show_time, 500 ms) and the
 * time-sync callback second; both are kept so the test can run them. */
#define MAX_TIMERS 4
static void (*timer_cbs[MAX_TIMERS])(lv_timer_t *);
static int    timer_count;

static uint64_t      now_ms = 1000;
static radio_state_t radio_state = RADIO_RX;

/* The index of a stub object in `labels`, so lv_label_set_text() writes the text
 * of the object it was given. */
static int label_index(const lv_obj_t * obj) {
    int i = (int) (obj - labels);

    return (i >= 0 && i < MAX_LABELS) ? i : 0;
}

/* Every call returns a different object: clock.c creates two labels (the shadow
 * and the value) and the test needs their texts apart. */
lv_obj_t * lv_label_create(lv_obj_t * parent) {
    (void) parent;

    if (label_count >= MAX_LABELS) {
        return &labels[MAX_LABELS - 1];
    }
    return &labels[label_count++];
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

void lv_obj_set_style_text_opa(lv_obj_t * obj, lv_opa_t value, uint32_t selector) {
    (void) obj;
    (void) value;
    (void) selector;
}

void lv_obj_set_style_bg_img_opa(lv_obj_t * obj, lv_opa_t value, uint32_t selector) {
    (void) obj;
    (void) value;
    (void) selector;
}

void lv_label_set_text(lv_obj_t * obj, const char * text) {
    snprintf(label_text[label_index(obj)], MAX_LABEL_TEXT, "%s", text);
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

/* El valor: la ULTIMA etiqueta que clock_init() crea. */
const char * clock_stub_last_text(void) {
    if (label_count == 0) {
        return "";
    }
    return label_text[label_count - 1];
}

/* El "8 fantasma": la PRIMERA etiqueta, la que queda debajo del valor. */
const char * clock_stub_shadow_text(void) {
    if (label_count < 2) {
        return "";
    }
    return label_text[0];
}

void clock_stub_run_timers(void) {
    for (int i = 0; i < timer_count; i++) {
        timer_cbs[i](&timer);
    }
}

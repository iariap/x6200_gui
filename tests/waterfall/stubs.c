/*
 * Stubs del test determinista del rasterizado incremental del waterfall
 * (OPT-2, issue #148). Compila el src/waterfall.c REAL contra LVGL 8.3.11 real
 * y sustituye solo lo que la radio aporta: params, cfg_cur/subjects, tema,
 * paleta, backlight y el scheduler.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"

#include "params/params.h"
#include "theme.h"
#include "styles.h"
#include "backlight.h"
#include "scheduler.h"
#include "band_info.h"
#include "util.h"
#include "test_hooks.h"

/* Reserva fallida del frame: el link va con --wrap=lv_img_buf_alloc y este
 * wrapper deja pasar la primera llamada como NULL cuando el test lo pide. */
bool test_fail_img_alloc;

lv_img_dsc_t *__real_lv_img_buf_alloc(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf);

lv_img_dsc_t *__wrap_lv_img_buf_alloc(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf) {
    if (test_fail_img_alloc) {
        test_fail_img_alloc = false;
        return NULL;
    }
    return __real_lv_img_buf_alloc(w, h, cf);
}

/* ---------------- params ---------------- */

params_t params;

/* ---------------- subjects ---------------- */

#define SUBJECT_MAX_OBSERVERS 8

struct Subject {
    int32_t  val;
    unsigned count;
    void    (*fn[SUBJECT_MAX_OBSERVERS])(Subject *, void *);
    void    *user_data[SUBJECT_MAX_OBSERVERS];
};

static Subject subjects[16];
static unsigned subject_next;

Subject *test_subject_create(int32_t initial) {
    Subject *s = &subjects[subject_next++ % (sizeof(subjects) / sizeof(subjects[0]))];

    memset(s, 0, sizeof(*s));
    s->val = initial;
    return s;
}

void *subject_add_observer_and_call(Subject *subj, void (*fn)(Subject *, void *), void *user_data) {
    if (subj == NULL) {
        return NULL;
    }

    if (subj->count < SUBJECT_MAX_OBSERVERS) {
        subj->fn[subj->count] = fn;
        subj->user_data[subj->count] = user_data;
        subj->count++;
    }

    fn(subj, user_data);   /* como el firmware: llama al observer al registrar */
    return NULL;
}

int32_t subject_get_int(Subject *subj) {
    return subj->val;
}

void subject_set_int(Subject *subj, int32_t val) {
    if (subj->val == val) {
        return;
    }

    subj->val = val;

    for (unsigned i = 0; i < subj->count; i++) {
        subj->fn[i](subj, subj->user_data[i]);
    }
}

cfg_cur_t cfg_cur;

/* ---------------- tema y paleta ---------------- */

theme_shape_t theme_shape = { .waterfall_signal_width = 3 };

uint32_t theme_hex(theme_color_t role) {
    (void) role;
    return 0xffffff;
}

lv_style_t waterfall_style;

static uint32_t test_palette[256];

const uint32_t *wf_palette = test_palette;

/* ---------------- backlight ---------------- */

static bool backlight_on = true;

bool backlight_is_on(void) {
    return backlight_on;
}

void test_backlight_set(bool on) {
    backlight_on = on;
}

/* ---------------- scheduler ---------------- */

#define SCHED_MAX 4096

static scheduler_fn_t sched_fn[SCHED_MAX];
static int            sched_count;

bool scheduler_put_noargs(scheduler_fn_t fn) {
    if (sched_count >= SCHED_MAX) {
        return false;
    }

    sched_fn[sched_count++] = fn;
    return true;
}

void scheduler_work(void) {
    int n = sched_count;

    sched_count = 0;

    for (int i = 0; i < n; i++) {
        sched_fn[i](NULL);
    }
}

/* ---------------- band info / util ---------------- */

void band_info_init(lv_obj_t *parent) {
    (void) parent;
}

void lpf(float *x, float current, float beta, float initial) {
    (void) initial;
    *x = *x * beta + current * (1.0f - beta);
}

/* ---------------- paleta de prueba ---------------- */

void test_palette_set(uint8_t index, uint32_t rgb) {
    test_palette[index] = rgb;
}

void test_palette_fill(uint32_t (*fn)(uint8_t)) {
    for (int i = 0; i < 256; i++) {
        test_palette[i] = fn((uint8_t) i);
    }
}

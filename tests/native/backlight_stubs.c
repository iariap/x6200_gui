/*
 * Stub implementations for the native backlight test (tests/native/test_backlight.c).
 *
 * They stand in for the firmware symbols src/backlight.c calls and, above all,
 * for the ONE the increment 4 rewire depends on:
 * visual_policy_brightness_rewrite() (the policy query the tick/timer rewrites
 * have to go through). It is a recorder: the test asserts that backlight_tick()
 * and the idle timer hand it the params level, so reverting the wiring back to a
 * direct backlight_set_brightness() call leaves the recorder empty and the test
 * red.
 *
 * The real src/backlight.c is compiled in this binary (staged next to these
 * headers), so backlight_set_brightness() is the module's own one: it writes to
 * sysfs, which does not exist on the host, and the fds stay -1, so the write
 * path is a no-op and the observation point is the recorder.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>
#include <stdint.h>

#include "lvgl/lvgl.h"
#include "params/params.h"
#include "gpio_safe.h"
#include "voice.h"
#include "visual_policy.h"

params_t params;

/* ---------------- la query de la politica (incremento 4) ---------------- */

static int     rewrite_calls;
static int16_t rewrite_last;
static bool    rewrite_last_valid;

void visual_policy_brightness_rewrite(int16_t user_setting) {
    rewrite_calls++;
    rewrite_last = user_setting;
    rewrite_last_valid = true;
}

int test_rewrite_calls(void) {
    return rewrite_calls;
}

int16_t test_rewrite_last(void) {
    return rewrite_last;
}

bool test_rewrite_last_valid(void) {
    return rewrite_last_valid;
}

void test_rewrite_reset(void) {
    rewrite_calls = 0;
    rewrite_last = 0;
    rewrite_last_valid = false;
}

/* ---------------- LVGL: timers que el test puede disparar ---------------- */

struct _lv_timer_t {
    lv_timer_cb_t cb;
    uint32_t      period;
};

static struct _lv_timer_t idle_timer;
static lv_timer_cb_t      idle_cb;

lv_timer_t * lv_timer_create(lv_timer_cb_t cb, uint32_t period, void * user_data) {
    (void) user_data;

    idle_cb = cb;
    idle_timer.cb = cb;
    idle_timer.period = period;

    return &idle_timer;
}

void lv_timer_set_period(lv_timer_t * timer, uint32_t period) {
    if (timer) {
        timer->period = period;
    }
}

void lv_timer_reset(lv_timer_t * timer) {
    (void) timer;
}

void lv_timer_set_repeat_count(lv_timer_t * timer, int32_t count) {
    (void) timer;
    (void) count;
}

void lv_disp_enable_invalidation(void * disp, bool en) {
    (void) disp;
    (void) en;
}

void * lv_disp_get_default(void) {
    return NULL;
}

/* Dispara el callback de idle que backlight_tick() registro (el timeout). */
void test_backlight_run_idle_timer(void) {
    if (idle_cb) {
        idle_cb(&idle_timer);
    }
}

/* ---------------- el resto del firmware que backlight.c toca ---------------- */

void gpio_safe_set(int pin, int value) {
    (void) pin;
    (void) value;
}

void voice_say_text_fmt(const char * fmt, ...) {
    (void) fmt;
}

void params_lock(void) {}

void params_unlock(bool * dirty) {
    if (dirty) {
        *dirty = true;
    }
}

/*
 * Minimal LVGL stub for the native backlight test (tests/native/test_backlight.c).
 *
 * Only the timer surface src/backlight.c calls, so the test can drive
 * backlight_tick() and then run the idle callback the module registered.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct _lv_timer_t lv_timer_t;
typedef void (*lv_timer_cb_t)(lv_timer_t *);

lv_timer_t *lv_timer_create(lv_timer_cb_t cb, uint32_t period, void *user_data);
void lv_timer_set_period(lv_timer_t *timer, uint32_t period);
void lv_timer_reset(lv_timer_t *timer);
void lv_timer_set_repeat_count(lv_timer_t *timer, int32_t count);

/* backlight_switch() (not the path under test, but linked) needs these. */
void  lv_disp_enable_invalidation(void *disp, bool en);
void *lv_disp_get_default(void);

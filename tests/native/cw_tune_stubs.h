/*
 * Capturing stubs for the native cw_tune_ui test (issue #131).
 *
 * src/cw_tune_ui.c is staged next to a fake LVGL and a fake scheduler (the same
 * pattern as the hkey/clock/backlight/spectrum tests). Every stubbed LVGL entry
 * point records the thread that called it: that is what turns "cw_tune_set_freq()
 * and cw_tune_ui_visible() must not touch LVGL from the audio thread" into an
 * assertion instead of a convention.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

void            cw_tune_stub_reset(void);
void            cw_tune_stub_lvgl_thread(pthread_t tid);

unsigned        cw_tune_stub_lvgl_calls(void);
unsigned        cw_tune_stub_lvgl_foreign_calls(void);
unsigned        cw_tune_stub_refresh_events(void);
unsigned        cw_tune_stub_refresh_foreign(void);

/* Cambia el modo y drena los observers diferidos (el lazo de GUI). */
void            cw_tune_stub_mode(int32_t mode);
void            cw_tune_stub_delayed_notify(void);

/* --- cola del scheduler (la real vive en src/scheduler.cpp) ---------------- */

unsigned        scheduler_pending(void);

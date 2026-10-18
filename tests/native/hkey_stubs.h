/*
 * Capturing stubs for the native hkey test (issue #131).
 *
 * src/hkey.c is staged next to a fake LVGL, a fake event queue, a fake backlight
 * and a fake scheduler (the same pattern as the clock/backlight/digital-modes/
 * SWR-Scan/restore/events tests). Every stubbed entry point records the thread
 * that called it: that is what turns "hkey_put() must not touch LVGL from the
 * radio thread" into an assertion instead of a convention.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "events.h"
#include "lvgl/lvgl.h"

/* --- registro de llamadas ------------------------------------------------- */

void            hkey_stub_reset(void);
void            hkey_stub_lvgl_thread(pthread_t tid);

unsigned        hkey_stub_lvgl_calls(void);
unsigned        hkey_stub_lvgl_foreign_calls(void);
unsigned        hkey_stub_backlight_ticks(void);
unsigned        hkey_stub_backlight_foreign(void);

unsigned        hkey_stub_events(void);
unsigned        hkey_stub_events_foreign(void);
event_hkey_t    hkey_stub_last_event(void);

unsigned        hkey_stub_keys(void);
unsigned        hkey_stub_keys_foreign(void);
int32_t         hkey_stub_last_key(void);

void            hkey_stub_set_editing(bool editing);

/* --- temporizadores (el de long-press) ------------------------------------ */

unsigned        hkey_stub_timers_created(void);
unsigned        hkey_stub_timers_deleted(void);
unsigned        hkey_stub_timers_live(void);
unsigned        hkey_stub_timers_double_del(void);
uint32_t        hkey_stub_last_timer_period(void);
int32_t         hkey_stub_last_timer_repeat(void);
bool            hkey_stub_timer_fire(void);

/* --- cola del scheduler (la real vive en src/scheduler.cpp) ---------------- */

unsigned        scheduler_pending(void);

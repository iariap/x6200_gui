/*
 * Host stubs for the events-queue test (issue #111).
 *
 * These stand in for the LVGL, keyboard and log symbols src/events.c uses, and
 * record what the queue did so the test can assert on it: dispatched events,
 * invalidated objects and LV_LOG_ERROR lines. The clock is controllable so the
 * rate-limited overflow log can be driven deterministically.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "stubs_events/lvgl/lvgl.h"
#include "stubs_events/keyboard.h"
#include "events_stubs.h"

static lv_obj_t     scr_obj;
static lv_group_t   focused_group;

lv_group_t *keyboard_group = &focused_group;

static int          dispatched;
static int          invalidated;
static int          log_lines;
static char         last_log[256];
static uint32_t     tick_ms;
static uint32_t     next_event_id = 100;

void events_stub_reset(void) {
    dispatched = 0;
    invalidated = 0;
    log_lines = 0;
    last_log[0] = '\0';
    tick_ms = 0;
}

void events_stub_tick_set(uint32_t ms) {
    tick_ms = ms;
}

int events_stub_dispatched(void) {
    return dispatched;
}

int events_stub_invalidated(void) {
    return invalidated;
}

int events_stub_log_lines(void) {
    return log_lines;
}

const char *events_stub_last_log(void) {
    return last_log;
}

uint32_t lv_event_register_id(void) {
    return next_event_id++;
}

lv_obj_t *lv_scr_act(void) {
    return &scr_obj;
}

void lv_obj_invalidate(lv_obj_t *obj) {
    (void) obj;
    invalidated++;
}

void lv_event_send(lv_obj_t *obj, lv_event_code_t code, void *param) {
    (void) obj;
    (void) code;
    (void) param;
    dispatched++;
}

lv_obj_t *lv_group_get_focused(const lv_group_t *group) {
    (void) group;
    return &scr_obj;
}

uint32_t lv_tick_get(void) {
    return tick_ms;
}

void lv_log_capture(const char *fmt, ...) {
    va_list args;

    log_lines++;
    va_start(args, fmt);
    vsnprintf(last_log, sizeof(last_log), fmt, args);
    va_end(args);
}

/*
 * Minimal LVGL stub for the native events-queue test (issue #111).
 *
 * Only the surface src/events.c touches: the event codes, the queue primitives
 * and a capturable LV_LOG_ERROR (the real LVGL prints through printf, so the
 * test cannot count lines otherwise). No rendering happens here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Concrete here (the stub instantiates them); opaque to the firmware. */
struct _lv_obj_t { int unused; };
struct _lv_group_t { int unused; };

typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_group_t lv_group_t;

/* Only the codes events.c names; the custom EVENT_* ids are registered at
 * runtime with lv_event_register_id() and carried verbatim by the queue. */
typedef enum {
    LV_EVENT_KEY = 0,
    LV_EVENT_REFRESH,
} lv_event_code_t;

/* Key codes named by src/events.h (values from LVGL's lv_group.h). */
typedef enum {
    LV_KEY_BACKSPACE = 8,
} lv_key_t;

uint32_t    lv_event_register_id(void);
lv_obj_t   *lv_scr_act(void);
void        lv_obj_invalidate(lv_obj_t *obj);
void        lv_event_send(lv_obj_t *obj, lv_event_code_t code, void *param);
lv_obj_t   *lv_group_get_focused(const lv_group_t *group);
uint32_t    lv_tick_get(void);

/* Host sink for the LVGL log: the real macro formats and printf()s, here each
 * line is captured so the test can assert the overflow log is rate-limited. */
void lv_log_capture(const char *fmt, ...);

#define LV_LOG_ERROR(fmt, ...) lv_log_capture(fmt, ##__VA_ARGS__)
#define LV_LOG_WARN(fmt, ...)  lv_log_capture(fmt, ##__VA_ARGS__)
#define LV_LOG_USER(fmt, ...)  lv_log_capture(fmt, ##__VA_ARGS__)
#define LV_LOG_INFO(fmt, ...)  do { } while (0)
#define LV_LOG_TRACE(fmt, ...) do { } while (0)

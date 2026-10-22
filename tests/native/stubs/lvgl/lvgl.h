/*
 * Minimal LVGL stub for the host/ARM unit tests.
 * Only the symbols used by adaptive_tuning.c, audio_enhancement.c and the
 * visual_policy.c wiring (waterfall.h needs lv_obj_t).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Controllable clock (set from the tests with test_tick_set()). */
uint32_t lv_tick_get(void);

/* Logging macros used by the ported modules (no output in the tests). */
#define LV_LOG_ERROR(fmt, ...) do { } while (0)
#define LV_LOG_WARN(fmt, ...)  do { } while (0)
#define LV_LOG_USER(fmt, ...)  do { } while (0)
#define LV_LOG_INFO(fmt, ...)  do { } while (0)
#define LV_LOG_TRACE(fmt, ...) do { } while (0)

/* The object type waterfall.h names (the real one comes from LVGL on target). */
typedef struct _lv_obj_t lv_obj_t;

/* Types theme.c needs (the real ones come from LVGL on the target) */
typedef int16_t          lv_coord_t;
typedef struct { uint8_t blue, green, red, alpha; } lv_color_t;
typedef struct { uint8_t blue, green, red, alpha; } lv_color32_t;
typedef struct { struct { uint32_t cf : 5; uint32_t always_zero : 3; uint32_t reserved : 2; uint32_t w : 11; uint32_t h : 11; } header; uint32_t data_size; const void *data; } lv_img_dsc_t;
#define LV_IMG_CF_TRUE_COLOR_ALPHA 5
#define LV_IMG_CF_TRUE_COLOR 4

/* Input device data (kbd_rollover.c) */
typedef enum { LV_INDEV_STATE_RELEASED = 0, LV_INDEV_STATE_PRESSED = 1 } lv_indev_state_t;
typedef struct { uint32_t key; lv_indev_state_t state; bool continue_reading; } lv_indev_data_t;

/* Event and log types dialog.h needs when params.c pulls it in through
 * dialog_msg_cw.h (issue #186 host stage: the real ones come from LVGL on the
 * target, but the host has no submodule). */
typedef struct _lv_event_t lv_event_t;
typedef void (*lv_event_cb_t)(lv_event_t * e);
typedef uint32_t lv_event_code_t;

typedef enum {
    LV_LOG_LEVEL_TRACE = 0,
    LV_LOG_LEVEL_INFO,
    LV_LOG_LEVEL_WARN,
    LV_LOG_LEVEL_ERROR,
    LV_LOG_LEVEL_USER,
    LV_LOG_LEVEL_NONE
} lv_log_level_t;

/*
 * Minimal LVGL stub for the host/ARM unit tests.
 * Only the symbols used by adaptive_tuning.c and audio_enhancement.c.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

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

/* Types theme.c needs (the real ones come from LVGL on the target) */
typedef int16_t          lv_coord_t;
typedef struct { uint8_t blue, green, red, alpha; } lv_color32_t;
typedef struct { struct { uint32_t cf : 5; uint32_t always_zero : 3; uint32_t reserved : 2; uint32_t w : 11; uint32_t h : 11; } header; uint32_t data_size; const void *data; } lv_img_dsc_t;
#define LV_IMG_CF_TRUE_COLOR_ALPHA 5

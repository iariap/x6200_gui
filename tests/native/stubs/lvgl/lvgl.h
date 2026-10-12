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

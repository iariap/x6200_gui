/*
 * Minimal LVGL stub for the native remote_server test (tests/native/test_remote.cpp).
 * The real src/remote_server.cpp only needs the logging macros and LV_MAX from LVGL.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include <stdint.h>

#define LV_LOG_ERROR(fmt, ...) do { } while (0)
#define LV_LOG_WARN(fmt, ...)  do { } while (0)
#define LV_LOG_USER(fmt, ...)  do { } while (0)
#define LV_LOG_INFO(fmt, ...)  do { } while (0)
#define LV_LOG_TRACE(fmt, ...) do { } while (0)

#ifndef LV_MAX
#define LV_MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef LV_MIN
#define LV_MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef struct _lv_obj_t lv_obj_t;
typedef int16_t          lv_coord_t;

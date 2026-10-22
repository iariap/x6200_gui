/*
 * Minimal LVGL surface for the native digital_modes test: once the module is
 * staged as src/cfg/digital_modes.c, the only LVGL it uses are the two log
 * macros. Like the pinned lvgl.h (which reaches <string.h> through
 * src/misc/lv_mem.h), this stub includes <string.h>, so strdup() is declared
 * exactly as it is in the firmware build.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <string.h>

void lv_stub_log(const char * level, const char * fmt, ...);

#define LV_LOG_ERROR(fmt, ...) lv_stub_log("ERROR", fmt, ##__VA_ARGS__)
#define LV_LOG_WARN(fmt, ...)  lv_stub_log("WARN", fmt, ##__VA_ARGS__)

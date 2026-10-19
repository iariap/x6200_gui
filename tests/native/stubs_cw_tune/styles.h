/*
 * styles.h stub for the native cw_tune_ui test (issue #131): the widget style
 * and the three fixed colours the tone blocks use. Both are plain objects in
 * the stub (see tests/native/cw_tune_stubs.c).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "lvgl/lvgl.h"

extern lv_style_t cw_tune_style;

#define COLOR_LIGHT_GREEN  (0x80FF80)
#define COLOR_LIGHT_YELLOW (0xFFFF30)
#define COLOR_LIGHT_RED    (0xFF8080)

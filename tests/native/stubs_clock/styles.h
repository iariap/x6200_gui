/*
 * Minimal styles.h for the native clock test (tests/native/test_clock.c).
 * src/clock.c uses clock_style, sony_30 and sony_38 plus the plug symbols.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "lvgl/lvgl.h"

#define SYMBOL_PLUG "\xEF\x87\xA6"        /* U+F1E6 */
#define SYMBOL_PLUG_CHARGE "\xEE\x95\x9B" /* U+E55B */

extern lv_style_t clock_style;
extern lv_font_t sony_30;
extern lv_font_t sony_38;

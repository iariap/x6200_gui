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

/* src/fonts/font_select.c: the family the digit panels draw with. The clock test
 * stubs it (clock_stubs.c) as the slot itself: no segmented digits there. */
const lv_font_t *font_digit(const lv_font_t *stock_slot);
void fonts_set_digit_family(uint8_t family);

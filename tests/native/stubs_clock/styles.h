/*
 * Minimal styles.h for the native clock test (tests/native/test_clock.c).
 * src/clock.c uses clock_style, sony_20, sony_22, sony_44 plus the plug symbols.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "lvgl/lvgl.h"

#define SYMBOL_PLUG "\xEF\x87\xA6"        /* U+F1E6 */
#define SYMBOL_PLUG_CHARGE "\xEE\x95\x9B" /* U+E55B */

extern lv_style_t clock_style;
extern lv_font_t sony_16;
extern lv_font_t sony_20;
extern lv_font_t sony_22;
extern lv_font_t sony_30;
extern lv_font_t sony_38;
extern lv_font_t sony_44;

/* Opacidad del "8 fantasma" (src/styles.h): el valor real es el mismo LV_OPA_30. */
#define UI_DIGIT_SHADOW_OPA LV_OPA_30

/* src/fonts/font_select.c: the family the digit panels draw with. The clock test
 * stubs both (clock_stubs.c): `font_digit()` is the slot itself and
 * `font_digit_active()` is true, so the clock builds the "8 fantasma" layer the
 * test checks. See the comment there. */
const lv_font_t *font_digit(const lv_font_t *stock_slot);
bool font_digit_active(void);
void fonts_set_digit_family(uint8_t family);

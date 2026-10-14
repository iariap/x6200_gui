/*
 * Minimal LVGL stub for the native clock test (tests/native/test_clock.c).
 *
 * Only the API src/clock.c touches. It is deliberately separate from
 * tests/native/stubs/lvgl/lvgl.h: that one serves the modules of the main
 * native suite (adaptive_tuning, audio_enhancement, theme, ...) and clock.c
 * needs a different surface, so the shared stub is left untouched.
 *
 * The LV_SYMBOL_BATTERY_* values are copied verbatim from the pinned LVGL
 * (lvgl/src/font/lv_symbol_def.h): the battery family is the U+F240..U+F244
 * block, `BATTERY_FULL` = U+F240 and `BATTERY_EMPTY` = U+F244.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef int16_t lv_coord_t;

typedef struct _lv_obj_t { int unused; } lv_obj_t;
typedef struct _lv_timer_t { int unused; } lv_timer_t;
typedef struct { int unused; } lv_style_t;
typedef struct { int unused; } lv_font_t;

typedef enum {
    LV_TEXT_ALIGN_LEFT = 0,
    LV_TEXT_ALIGN_CENTER,
    LV_TEXT_ALIGN_RIGHT,
    LV_TEXT_ALIGN_AUTO
} lv_text_align_t;

lv_obj_t * lv_label_create(lv_obj_t * parent);
void lv_obj_add_style(lv_obj_t * obj, const lv_style_t * style, uint32_t selector);
void lv_obj_set_style_text_font(lv_obj_t * obj, const lv_font_t * font, uint32_t selector);
void lv_obj_set_style_pad_ver(lv_obj_t * obj, lv_coord_t value, uint32_t selector);
void lv_obj_set_style_text_align(lv_obj_t * obj, lv_text_align_t align, uint32_t selector);
void lv_label_set_text(lv_obj_t * obj, const char * text);
lv_timer_t * lv_timer_create(void (*timer_cb)(lv_timer_t *), uint32_t period, void * user_data);

#define LV_SYMBOL_BATTERY_FULL  "\xEF\x89\x80" /* U+F240 */
#define LV_SYMBOL_BATTERY_3     "\xEF\x89\x81" /* U+F241 */
#define LV_SYMBOL_BATTERY_2     "\xEF\x89\x82" /* U+F242 */
#define LV_SYMBOL_BATTERY_1     "\xEF\x89\x83" /* U+F243 */
#define LV_SYMBOL_BATTERY_EMPTY "\xEF\x89\x84" /* U+F244 */

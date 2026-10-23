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
 * block, `BATTERY_FULL` = U+F240 and `BATTERY_EMPTY` = U+F244. The opacity enum
 * is copied verbatim from lvgl/src/misc/lv_color.h (LV_OPA_30 = 76): src/clock.c
 * and the style token it uses (UI_DIGIT_SHADOW_OPA) need it.
 *
 * Issue #250 (reloj permanente + banda de estado): the clock builds a container
 * plus per-role labels, so the stub also mirrors the object API that structure
 * needs (`lv_obj_create`, the size/align setters, the flags and the resolved
 * style getters the equality guards read). The geometry itself is measured with
 * the real LVGL in tools/ui_harness (`--inspect`); here the stub records the
 * calls so the test can pin what each label asks for.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef int16_t lv_coord_t;
typedef uint8_t lv_opa_t;
typedef uint8_t lv_align_t;

typedef struct _lv_obj_t { int unused; } lv_obj_t;
typedef struct _lv_timer_t { int unused; } lv_timer_t;
typedef struct { int unused; } lv_style_t;
typedef struct { int unused; } lv_font_t;

/* lvgl/src/misc/lv_color.h: the palette of the ROM is 32-bit true colour. */
typedef struct { uint8_t blue, green, red, alpha; } lv_color_t;

typedef enum {
    LV_TEXT_ALIGN_LEFT = 0,
    LV_TEXT_ALIGN_CENTER,
    LV_TEXT_ALIGN_RIGHT,
    LV_TEXT_ALIGN_AUTO
} lv_text_align_t;

/* lvgl/src/misc/lv_area.h */
enum {
    LV_ALIGN_TOP_LEFT = 0,
    LV_ALIGN_TOP_MID,
    LV_ALIGN_TOP_RIGHT,
    LV_ALIGN_BOTTOM_LEFT,
    LV_ALIGN_BOTTOM_MID,
    LV_ALIGN_BOTTOM_RIGHT,
    LV_ALIGN_LEFT_MID,
    LV_ALIGN_RIGHT_MID,
    LV_ALIGN_CENTER,
    LV_ALIGN_OUT_TOP_LEFT,
    LV_ALIGN_OUT_TOP_MID,
    LV_ALIGN_OUT_TOP_RIGHT,
    LV_ALIGN_OUT_BOTTOM_LEFT,
    LV_ALIGN_OUT_BOTTOM_MID,
    LV_ALIGN_OUT_BOTTOM_RIGHT,
    LV_ALIGN_OUT_LEFT_TOP,
    LV_ALIGN_OUT_LEFT_MID,
    LV_ALIGN_OUT_LEFT_BOTTOM,
    LV_ALIGN_OUT_RIGHT_TOP,
    LV_ALIGN_OUT_RIGHT_MID,
    LV_ALIGN_OUT_RIGHT_BOTTOM,
};

/* lvgl/src/misc/lv_area.h with LV_USE_LARGE_COORD off (lv_conf.h): the type
 * specifier lives in bit 13, so `LV_SIZE_CONTENT` and `lv_pct(100)` are the
 * values the real header computes. The stub keeps the arithmetic instead of the
 * literals so the test reads the same number src/clock.c asks for. */
#define _LV_COORD_TYPE_SHIFT (13U)
#define LV_COORD_SET_SPEC(x) ((lv_coord_t)((x) | (1 << _LV_COORD_TYPE_SHIFT)))
#define LV_PCT(x)            (x < 0 ? LV_COORD_SET_SPEC(1000 - (x)) : LV_COORD_SET_SPEC(x))
#define LV_SIZE_CONTENT      LV_COORD_SET_SPEC(2001)

static inline lv_coord_t lv_pct(lv_coord_t x)
{
    return LV_PCT(x);
}

/* lvgl/src/core/lv_obj.h: the two flags src/clock.c clears (the clock panel is
 * not a control: it captures no input and does not scroll) and the one it sets
 * on the status line when there is no external supply. */
enum {
    LV_OBJ_FLAG_HIDDEN          = 1 << 1,
    LV_OBJ_FLAG_CLICKABLE       = 1 << 2,
    LV_OBJ_FLAG_SCROLLABLE      = 1 << 4,
    LV_OBJ_FLAG_CLICK_FOCUSABLE = 1 << 10,
};

/* lvgl/src/misc/lv_color.h */
enum {
    LV_OPA_TRANSP = 0,
    LV_OPA_0      = 0,
    LV_OPA_10     = 25,
    LV_OPA_20     = 51,
    LV_OPA_30     = 76,
    LV_OPA_40     = 102,
    LV_OPA_50     = 127,
    LV_OPA_60     = 153,
    LV_OPA_70     = 178,
    LV_OPA_80     = 204,
    LV_OPA_90     = 229,
    LV_OPA_100    = 255,
    LV_OPA_COVER  = 255,
};

#define LV_OPA_MIN 2 /*Opacities below this will be transparent*/

lv_color_t lv_color_hex(uint32_t c);

/* lvgl/src/misc/lv_color.h (LV_COLOR_DEPTH 32): src/clock.c compara el estilo
 * resuelto para no reescribir un color que no cambio. */
static inline uint32_t lv_color_to32(lv_color_t color)
{
    return ((uint32_t) color.red << 16) | ((uint32_t) color.green << 8) | (uint32_t) color.blue;
}

lv_obj_t * lv_obj_create(lv_obj_t * parent);
lv_obj_t * lv_label_create(lv_obj_t * parent);
void lv_obj_remove_style_all(lv_obj_t * obj);
void lv_obj_add_style(lv_obj_t * obj, const lv_style_t * style, uint32_t selector);
void lv_obj_add_flag(lv_obj_t * obj, uint32_t flag);
void lv_obj_clear_flag(lv_obj_t * obj, uint32_t flag);
bool lv_obj_has_flag(const lv_obj_t * obj, uint32_t flag);
void lv_obj_set_size(lv_obj_t * obj, lv_coord_t w, lv_coord_t h);
void lv_obj_align(lv_obj_t * obj, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs);
void lv_obj_align_to(lv_obj_t * obj, const lv_obj_t * base, lv_align_t align, lv_coord_t x_ofs,
                     lv_coord_t y_ofs);
void lv_obj_set_style_text_font(lv_obj_t * obj, const lv_font_t * font, uint32_t selector);
void lv_obj_set_style_text_color(lv_obj_t * obj, lv_color_t color, uint32_t selector);
void lv_obj_set_style_text_align(lv_obj_t * obj, lv_text_align_t align, uint32_t selector);
void lv_obj_set_style_text_opa(lv_obj_t * obj, lv_opa_t value, uint32_t selector);
void lv_obj_set_style_bg_img_opa(lv_obj_t * obj, lv_opa_t value, uint32_t selector);
void lv_obj_set_style_bg_img_src(lv_obj_t * obj, const void * src, uint32_t selector);
/* La banda de estado del #285 (src/clock.c): el fondo opaco del rol `bar`. */
void lv_obj_set_style_bg_color(lv_obj_t * obj, lv_color_t color, uint32_t selector);
void lv_obj_set_style_bg_opa(lv_obj_t * obj, lv_opa_t value, uint32_t selector);
/* API de la version previa del reloj (issues #9/#150: el `pad_ver 18` del valor y
 * su fantasma). El reloj del #250 ya no la usa, pero el stub la conserva: es el
 * unico camino para compilar la revision ANTERIOR de src/clock.c contra este
 * mismo test (la A/B del #250 y su prueba de rojo del banco nativo). */
void lv_obj_set_style_pad_ver(lv_obj_t * obj, lv_coord_t value, uint32_t selector);
/* OPT-4 #150: src/clock.c lee el estilo y el texto VIVOS del objeto para no
 * reescribir (ni invalidar) lo que no cambio; el stub los devuelve (ver
 * clock_stubs.c). */
const lv_font_t * lv_obj_get_style_text_font(const lv_obj_t * obj, uint32_t selector);
lv_color_t lv_obj_get_style_text_color(const lv_obj_t * obj, uint32_t selector);
lv_coord_t lv_obj_get_style_pad_top(const lv_obj_t * obj, uint32_t selector);
const void * lv_obj_get_style_bg_img_src(const lv_obj_t * obj, uint32_t selector);
const char * lv_label_get_text(const lv_obj_t * obj);
void lv_label_set_text(lv_obj_t * obj, const char * text);
lv_timer_t * lv_timer_create(void (*timer_cb)(lv_timer_t *), uint32_t period, void * user_data);

#define LV_SYMBOL_BATTERY_FULL  "\xEF\x89\x80" /* U+F240 */
#define LV_SYMBOL_BATTERY_3     "\xEF\x89\x81" /* U+F241 */
#define LV_SYMBOL_BATTERY_2     "\xEF\x89\x82" /* U+F242 */
#define LV_SYMBOL_BATTERY_1     "\xEF\x89\x83" /* U+F243 */
#define LV_SYMBOL_BATTERY_EMPTY "\xEF\x89\x84" /* U+F244 */

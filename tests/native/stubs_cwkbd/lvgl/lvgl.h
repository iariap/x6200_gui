/*
 * Minimal LVGL stub for the native CW keyboard dialog test (issue #217).
 *
 * Only the surface src/dialog_cw_kbd.c uses: objects, labels, the text area,
 * groups, the timer and the styles it reads and writes. No rendering happens
 * here, but the stub *counts* the two calls that make LVGL invalidate an object
 * (src/update_view sets a label text and/or a style every 100 ms): that count is
 * the measurement of the idle-render defect and of its fix.
 *
 * tests/native/stubs_cwkbd/ shadows the headers of src/ for the staged copy of
 * src/dialog_cw_kbd.c (the clock/backlight/SWR-Scan/cw_encoder pattern).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_group_t lv_group_t;
typedef struct _lv_timer_t lv_timer_t;
typedef struct _lv_event_t lv_event_t;
typedef struct _lv_indev_t lv_indev_t;

typedef void (*lv_event_cb_t)(lv_event_t * e);

/* 32-bit colour, same field order LVGL keeps internally (blue, green, red). */
typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} lv_color_t;

typedef struct _lv_font_t {
    int unused;
} lv_font_t;

typedef enum {
    LV_INDEV_TYPE_NONE = 0,
    LV_INDEV_TYPE_POINTER,
    LV_INDEV_TYPE_KEYPAD,
    LV_INDEV_TYPE_ENCODER,
    LV_INDEV_TYPE_BUTTON,
} lv_indev_type_t;

/* Event codes (only the two the dialog registers) */
#define LV_EVENT_KEY      12
#define LV_EVENT_CLICKED  7

/* Object flags */
#define LV_OBJ_FLAG_SCROLLABLE  (1 << 4)
#define LV_OBJ_FLAG_CLICKABLE   (1 << 5)

/* States and parts */
#define LV_STATE_ANY      0xFFFFu
#define LV_STATE_FOCUSED  0x0002u
#define LV_PART_MAIN      0x000000u
#define LV_PART_CURSOR    0x010000u

/* Opacities */
#define LV_OPA_COVER  255
#define LV_OPA_80     204

/* Text alignment */
#define LV_TEXT_ALIGN_CENTER  1
#define LV_TEXT_ALIGN_RIGHT   2

/* Alignment */
#define LV_ALIGN_BOTTOM_LEFT  4

/* Label long modes */
#define LV_LABEL_LONG_WRAP  2

/* Keys */
#define LV_KEY_ENTER      10
#define LV_KEY_ESC        27
#define LV_KEY_BACKSPACE  8
#define LV_KEY_UP         17
#define LV_KEY_DOWN       18
#define LV_KEY_RIGHT      19
#define LV_KEY_LEFT       20

lv_color_t lv_color_hex(uint32_t c);
uint32_t   lv_color_to32(lv_color_t color);

lv_obj_t *lv_obj_create(lv_obj_t *parent);
void      lv_obj_remove_style_all(lv_obj_t *obj);
void      lv_obj_remove_style(lv_obj_t *obj, void *style, uint32_t selector);
void      lv_obj_set_size(lv_obj_t *obj, int32_t w, int32_t h);
void      lv_obj_set_width(lv_obj_t *obj, int32_t w);
void      lv_obj_set_pos(lv_obj_t *obj, int32_t x, int32_t y);
void      lv_obj_align(lv_obj_t *obj, int align, int32_t x, int32_t y);
void      lv_obj_clear_flag(lv_obj_t *obj, uint32_t flag);
void      lv_obj_add_state(lv_obj_t *obj, uint32_t state);
void      lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, int filter, void *user_data);

void lv_obj_set_style_text_font(lv_obj_t *obj, const lv_font_t *font, uint32_t part);
void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t color, uint32_t part);
void lv_obj_set_style_text_align(lv_obj_t *obj, int align, uint32_t part);
void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t color, uint32_t part);
void lv_obj_set_style_bg_opa(lv_obj_t *obj, uint8_t opa, uint32_t part);
void lv_obj_set_style_border_color(lv_obj_t *obj, lv_color_t color, uint32_t part);
void lv_obj_set_style_border_width(lv_obj_t *obj, int32_t width, uint32_t part);
void lv_obj_set_style_radius(lv_obj_t *obj, int32_t radius, uint32_t part);
void lv_obj_set_style_pad_all(lv_obj_t *obj, int32_t pad, uint32_t part);
void lv_obj_set_style_pad_hor(lv_obj_t *obj, int32_t pad, uint32_t part);
void lv_obj_set_style_pad_top(lv_obj_t *obj, int32_t pad, uint32_t part);

lv_obj_t *lv_label_create(lv_obj_t *parent);
void      lv_label_set_text(lv_obj_t *obj, const char *text);
void      lv_label_set_text_fmt(lv_obj_t *obj, const char *fmt, ...);
void      lv_label_set_recolor(lv_obj_t *obj, bool en);
void      lv_label_set_long_mode(lv_obj_t *obj, int mode);
const char *lv_label_get_text(lv_obj_t *obj);

lv_obj_t   *lv_textarea_create(lv_obj_t *parent);
const char *lv_textarea_get_text(lv_obj_t *obj);
void        lv_textarea_set_text(lv_obj_t *obj, const char *text);
void        lv_textarea_add_text(lv_obj_t *obj, const char *text);
void        lv_textarea_del_char(lv_obj_t *obj);
void        lv_textarea_set_one_line(lv_obj_t *obj, bool en);
void        lv_textarea_set_max_length(lv_obj_t *obj, uint32_t max);
void        lv_textarea_set_placeholder_text(lv_obj_t *obj, const char *text);

void lv_group_add_obj(lv_group_t *group, lv_obj_t *obj);
void lv_group_remove_obj(lv_obj_t *obj);
void lv_group_focus_obj(lv_obj_t *obj);
void lv_group_set_editing(lv_group_t *group, bool edit);

lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *), uint32_t period, void *user_data);
void        lv_timer_del(lv_timer_t *timer);

lv_indev_t     *lv_indev_get_act(void);
lv_indev_type_t lv_indev_get_type(lv_indev_t *indev);
void           *lv_event_get_param(lv_event_t *e);

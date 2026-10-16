/*
 * Minimal LVGL stub for the native dialog restore test (issue #94).
 *
 * Only the surface src/dialog.c touches. No rendering happens here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef int16_t lv_coord_t;

typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_group_t lv_group_t;
typedef struct _lv_event_t lv_event_t;
typedef void (*lv_event_cb_t)(lv_event_t * e);

typedef uint32_t lv_style_t;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} lv_color_t;

typedef enum {
    LV_EVENT_KEY = 0,
    LV_EVENT_REFRESH,
} lv_event_code_t;

#define LV_OBJ_FLAG_SCROLLABLE  0x01
#define LV_OBJ_FLAG_SCROLL_ON_FOCUS 0x02
#define LV_STATE_DEFAULT        0x0000
#define LV_STATE_FOCUSED        0x0002
#define LV_STATE_EDITED         0x0004
#define LV_STATE_FOCUS_KEY      0x0008
#define LV_PART_CURSOR          0x10
#define LV_PART_INDICATOR       0x20
#define LV_OPA_TRANSP           0
#define LV_OPA_COVER            255

lv_obj_t *lv_obj_create(lv_obj_t *parent);
void      lv_obj_del(lv_obj_t *obj);
void      lv_obj_remove_style_all(lv_obj_t *obj);
void      lv_obj_add_style(lv_obj_t *obj, const lv_style_t *style, uint32_t selector);
void      lv_obj_clear_flag(lv_obj_t *obj, uint32_t flag);
void      lv_obj_add_flag(lv_obj_t *obj, uint32_t flag);
void      lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t filter, void *user_data);
void      lv_group_add_obj(lv_group_t *group, lv_obj_t *obj);
void      lv_obj_set_style_bg_opa(lv_obj_t *obj, uint8_t opa, uint32_t part);
void      lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t color, uint32_t part);
void      lv_obj_set_style_border_width(lv_obj_t *obj, lv_coord_t width, uint32_t part);
void      lv_obj_set_style_border_color(lv_obj_t *obj, lv_color_t color, uint32_t part);

lv_color_t lv_color_white(void);
lv_color_t lv_color_black(void);

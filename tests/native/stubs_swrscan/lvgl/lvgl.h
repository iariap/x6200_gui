/*
 * Minimal LVGL stub for the native SWR Scan dialog test (issues #85 and #119).
 *
 * Only the surface src/dialog_swrscan.c touches: the object/event/draw types it
 * reads and the calls it makes. No rendering happens here, but since #119 the
 * test *drives* the dialog's draw callback (swrscan_stub_draw() in
 * tests/native/swrscan_stubs.c) and reads the line segments it paints, so the
 * stub records the object geometry, the styles set on it and the segments.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef int16_t lv_coord_t;

typedef struct {
    lv_coord_t x;
    lv_coord_t y;
} lv_point_t;

typedef struct {
    lv_coord_t x1;
    lv_coord_t y1;
    lv_coord_t x2;
    lv_coord_t y2;
} lv_area_t;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} lv_color_t;

typedef struct _lv_font_t lv_font_t;
typedef struct _lv_group_t lv_group_t;

/* dialog_swrscan.c reads obj->coords directly in its draw callback. The fields
 * below it are recorded by the stub (lv_obj_set_size / the style setters) and read
 * by the test; the dialog itself never touches them. */
typedef struct _lv_obj_t {
    lv_area_t coords;
    lv_coord_t w;
    lv_coord_t h;
    uint8_t    bg_opa;
    bool       bg_color_set;
} lv_obj_t;

typedef struct _lv_event_t lv_event_t;
typedef void (*lv_event_cb_t)(lv_event_t * e);

typedef struct {
    int unused;
} lv_draw_ctx_t;

typedef struct {
    lv_color_t color;
    lv_coord_t width;
} lv_draw_line_dsc_t;

typedef struct {
    lv_color_t    color;
    const lv_font_t *font;
} lv_draw_label_dsc_t;

typedef enum {
    LV_EVENT_DELETE = 0,
    LV_EVENT_KEY,
    LV_EVENT_REFRESH,
    LV_EVENT_DRAW_MAIN_END,
} lv_event_code_t;

#define LV_PART_MAIN     0
#define LV_OPA_TRANSP    0
#define LV_OPA_COVER     255
#define LV_COORD_MAX     32767
#define LV_KEY_ESC       27

lv_obj_t *lv_obj_create(lv_obj_t *parent);
void      lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t filter, void *user_data);
void      lv_obj_set_size(lv_obj_t *obj, lv_coord_t w, lv_coord_t h);
void      lv_obj_center(lv_obj_t *obj);
void      lv_obj_set_style_bg_opa(lv_obj_t *obj, uint8_t opa, uint32_t part);
void      lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t color, uint32_t part);
void      lv_obj_set_style_border_width(lv_obj_t *obj, lv_coord_t width, uint32_t part);
void      lv_group_add_obj(lv_group_t *group, lv_obj_t *obj);
lv_coord_t lv_obj_get_width(lv_obj_t *obj);
lv_coord_t lv_obj_get_height(lv_obj_t *obj);
void      lv_obj_invalidate(lv_obj_t *obj);

lv_obj_t      *lv_event_get_target(lv_event_t *e);
lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t *e);
void          *lv_event_get_param(lv_event_t *e);

void lv_draw_line_dsc_init(lv_draw_line_dsc_t *dsc);
void lv_draw_label_dsc_init(lv_draw_label_dsc_t *dsc);
lv_color_t lv_color_white(void);
lv_color_t lv_color_hex(uint32_t c);
void lv_draw_line(lv_draw_ctx_t *ctx, const lv_draw_line_dsc_t *dsc, const lv_point_t *p1, const lv_point_t *p2);
void lv_draw_label(lv_draw_ctx_t *ctx, const lv_draw_label_dsc_t *dsc, const lv_area_t *coords, const char *txt, void *bidi);
void lv_txt_get_size(lv_point_t *size_res, const char *txt, const lv_font_t *font,
                     lv_coord_t letter_space, lv_coord_t line_space, lv_coord_t max_width, uint32_t flag);

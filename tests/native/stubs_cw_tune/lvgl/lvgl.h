/*
 * Minimal LVGL stub for the native cw_tune_ui test (issue #131).
 *
 * Only the surface src/cw_tune_ui.c touches: the widget it creates, the draw
 * event, the draw descriptors and the refresh event. Every entry point is
 * implemented in tests/native/cw_tune_stubs.c, which records the calling
 * thread: that is what turns "cw_tune_set_freq() / cw_tune_ui_visible() must not
 * touch LVGL from the audio thread" into an assertion instead of a convention.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int16_t lv_coord_t;

/* Concrete here (the stub instantiates them); opaque to the firmware. */
typedef struct {
    uint32_t full;
} lv_color_t;

typedef union {
    uint32_t full;
    struct {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
        uint8_t alpha;
    } ch;
} lv_color32_t;

typedef struct {
    lv_coord_t x1;
    lv_coord_t y1;
    lv_coord_t x2;
    lv_coord_t y2;
} lv_area_t;

typedef struct {
    lv_color_t bg_color;
    lv_color_t border_color;
    uint8_t    bg_opa;
    uint8_t    border_opa;
    uint8_t    border_width;
    uint8_t    radius;
} lv_draw_rect_dsc_t;

typedef struct _lv_obj_t {
    struct _lv_obj_t *parent;
    uint32_t          flags;
} lv_obj_t;

typedef struct _lv_draw_ctx_t {
    int unused;
} lv_draw_ctx_t;

typedef struct _lv_event_t {
    lv_obj_t      *target;
    lv_draw_ctx_t *draw_ctx;
} lv_event_t;

typedef struct _lv_style_t {
    int unused;
} lv_style_t;

typedef uint32_t lv_event_code_t;

typedef void (*lv_event_cb_t)(lv_event_t *e);

enum {
    LV_EVENT_DRAW_MAIN = 1,
    LV_EVENT_DELETE,
    LV_EVENT_REFRESH,
};

#define LV_OBJ_FLAG_HIDDEN      (1u << 0)
#define LV_OBJ_FLAG_CLICKABLE   (1u << 1)
#define LV_OBJ_FLAG_SCROLLABLE  (1u << 2)

#define LV_OPA_TRANSP   0
#define LV_OPA_50       128
#define LV_OPA_70       179
#define LV_OPA_COVER    255

#define LV_ABS(x)       ((x) > 0 ? (x) : -(x))

lv_obj_t      *lv_obj_create(lv_obj_t *parent);
void           lv_obj_set_height(lv_obj_t *obj, lv_coord_t h);
void           lv_obj_set_width(lv_obj_t *obj, lv_coord_t w);
void           lv_obj_add_flag(lv_obj_t *obj, uint32_t flag);
void           lv_obj_clear_flag(lv_obj_t *obj, uint32_t flag);
bool           lv_obj_has_flag(const lv_obj_t *obj, uint32_t flag);
void           lv_obj_add_style(lv_obj_t *obj, const lv_style_t *style, uint32_t part);
void           lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t code, void *user_data);
void           lv_obj_get_coords(const lv_obj_t *obj, lv_area_t *coords);
lv_obj_t      *lv_obj_get_parent(const lv_obj_t *obj);
lv_color_t     lv_obj_get_style_bg_color(const lv_obj_t *obj, uint32_t part);
lv_obj_t      *lv_event_get_target(lv_event_t *e);
lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t *e);
void           lv_event_send(lv_obj_t *obj, lv_event_code_t code, void *param);
void           lv_area_move(lv_area_t *area, lv_coord_t dx, lv_coord_t dy);
void           lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *dsc);
void           lv_draw_rect(lv_draw_ctx_t *ctx, const lv_draw_rect_dsc_t *dsc, const lv_area_t *area);

lv_color_t     lv_color_hex(uint32_t c);
lv_color_t     lv_color_white(void);
lv_color_t     lv_color_black(void);
lv_color_t     lv_color_make(uint8_t r, uint8_t g, uint8_t b);
uint32_t       lv_color_to32(lv_color_t color);

#define LV_PART_MAIN  0

/*
 * Minimal LVGL stub for the native spectrum test (issue #149).
 *
 * Only the surface src/spectrum.c touches: the object it creates, the draw
 * event, the draw descriptors and the three primitives of the callback
 * (lv_draw_line, lv_draw_rect, lv_obj_invalidate). Every entry point is
 * implemented in tests/native/spectrum_stubs.c, which records the calling
 * thread and the exact geometry drawn, so the test can reconstruct what the
 * callback painted and from which frame generation.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef int16_t lv_coord_t;

/* Concrete here (the stub instantiates them); opaque to the firmware. */
typedef struct {
    uint32_t full;
} lv_color_t;

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
    lv_color_t color;
    lv_coord_t width;
    uint8_t    opa;
} lv_draw_line_dsc_t;

typedef struct {
    lv_color_t bg_color;
    uint8_t    bg_opa;
} lv_draw_rect_dsc_t;

/* Fuente opaca para el test: src/spectrum.c solo pasa el puntero a
 * lv_txt_get_size()/lv_draw_label(). El stub le da una altura de linea fija
 * para que la medida del texto sea determinista. */
typedef struct _lv_font_t {
    lv_coord_t line_height;
} lv_font_t;

typedef struct {
    lv_color_t       color;
    const lv_font_t *font;
    uint8_t          opa;
    uint8_t          align;
} lv_draw_label_dsc_t;

typedef struct _lv_obj_t {
    lv_area_t  coords;
    lv_coord_t w;
    lv_coord_t h;
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

/* The firmware's own event ids are uint32 (events.c registers them with
 * lv_event_register_id()), so the stub keeps the same width. */
typedef uint32_t lv_event_code_t;

typedef void (*lv_event_cb_t)(lv_event_t *e);

enum {
    LV_EVENT_DRAW_MAIN_END = 1,
    LV_EVENT_KEY,
};

#define LV_OPA_0    0
#define LV_OPA_20   51
#define LV_OPA_50   128

#define LV_COORD_MAX 32767

#define LV_TEXT_ALIGN_CENTER 1

#define LV_MAX(a, b) ((a) > (b) ? (a) : (b))

lv_obj_t      *lv_obj_create(lv_obj_t *parent);
void           lv_obj_add_style(lv_obj_t *obj, const lv_style_t *style, uint32_t part);
void           lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t code, void *user_data);
lv_obj_t      *lv_event_get_target(lv_event_t *e);
lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t *e);
lv_coord_t     lv_obj_get_width(const lv_obj_t *obj);
lv_coord_t     lv_obj_get_height(const lv_obj_t *obj);
void           lv_draw_line_dsc_init(lv_draw_line_dsc_t *dsc);
void           lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *dsc);
void           lv_draw_line(lv_draw_ctx_t *ctx, const lv_draw_line_dsc_t *dsc,
                            const lv_point_t *p1, const lv_point_t *p2);
void           lv_draw_rect(lv_draw_ctx_t *ctx, const lv_draw_rect_dsc_t *dsc,
                            const lv_area_t *area);
void           lv_obj_invalidate(lv_obj_t *obj);
lv_color_t     lv_color_hex(uint32_t c);
lv_color_t     lv_color_white(void);
void           lv_draw_label_dsc_init(lv_draw_label_dsc_t *dsc);
void           lv_draw_label(lv_draw_ctx_t *ctx, const lv_draw_label_dsc_t *dsc,
                              const lv_area_t *coords, const char *txt, void *hint);
void           lv_txt_get_size(lv_point_t *size_res, const char *text, const lv_font_t *font,
                               lv_coord_t letter_space, lv_coord_t line_space,
                               lv_coord_t max_width, uint32_t flag);

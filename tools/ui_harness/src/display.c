/*
 *  Headless RAM framebuffer + LVGL flush callback (see display.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>

#include "display.h"
#include "render_metrics.h"

static lv_color_t fb[HARNESS_SCREEN_W * HARNESS_SCREEN_H];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px)
{
    uint32_t pixels = 0;

    for (int32_t y = area->y1; y <= area->y2; y++) {
        for (int32_t x = area->x1; x <= area->x2; x++) {
            fb[y * HARNESS_SCREEN_W + x] = *px++;
            pixels++;
        }
    }

    render_metrics_record_flush(pixels);
    lv_disp_flush_ready(drv);
}

void display_init(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t      drv;

    lv_color_t *buf1 = malloc(sizeof(lv_color_t) * HARNESS_SCREEN_W * HARNESS_SCREEN_H);

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, HARNESS_SCREEN_W * HARNESS_SCREEN_H);

    lv_disp_drv_init(&drv);
    drv.draw_buf = &draw_buf;
    drv.flush_cb = flush_cb;
    drv.hor_res  = HARNESS_SCREEN_W;
    drv.ver_res  = HARNESS_SCREEN_H;
    lv_disp_drv_register(&drv);
}

const lv_color_t *display_framebuffer(void)
{
    return fb;
}

uint32_t display_width(void)
{
    return HARNESS_SCREEN_W;
}

uint32_t display_height(void)
{
    return HARNESS_SCREEN_H;
}

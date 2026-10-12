/*
 * Headless LVGL render probe: draws a screen into a RAM framebuffer and writes
 * it to PNG. This is the core of a UI harness (see docs/UI_HARNESS.md): it lets
 * the firmware's real screens be checked without the radio.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lvgl/lvgl.h"

#define W 800
#define H 480

static lv_color_t fb[W * H];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            fb[y * W + x] = *px++;
        }
    }

    lv_disp_flush_ready(drv);
}

static void write_png(const char *path) {
    FILE *f = fopen(path, "wb");

    if (!f) {
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   info = png_create_info_struct(png);

    png_init_io(png, f);
    png_set_IHDR(png, info, W, H, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    uint8_t *row = malloc(W * 3);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            row[x * 3 + 0] = fb[y * W + x].ch.red;
            row[x * 3 + 1] = fb[y * W + x].ch.green;
            row[x * 3 + 2] = fb[y * W + x].ch.blue;
        }

        png_write_row(png, row);
    }

    free(row);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(f);
    printf("escrito %s\n", path);
}

int main(void) {
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    lv_color_t *buf1 = malloc(W * H * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, W * H);

    static lv_disp_drv_t drv;

    lv_disp_drv_init(&drv);
    drv.draw_buf = &draw_buf;
    drv.flush_cb = flush_cb;
    drv.hor_res  = W;
    drv.ver_res  = H;
    lv_disp_drv_register(&drv);

    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *l = lv_label_create(scr);

    lv_label_set_text(l, "X6200 UI harness\nrender headless OK");
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_center(l);

    for (int i = 0; i < 6; i++) {
        lv_timer_handler();
        usleep(20000);
    }

    write_png("probe.png");

    return 0;
}

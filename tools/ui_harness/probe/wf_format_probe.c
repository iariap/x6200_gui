/*
 *  Formato de imagen del waterfall (OPT-2, issue #148): coste de dibujo de una
 *  imagen TRUE_COLOR de 32 bits contra una INDEXED_8BIT + paleta.
 *
 *  Why it exists: la opcion B del issue propone guardar la imagen como indices
 *  uint8_t y convertir con la paleta al dibujar. Antes de implementarla hay que
 *  saber si el flush de LVGL 8.3.11 de una imagen indexada es competitivo con el
 *  `memcpy` del TRUE_COLOR de 32 bits (lv_draw_sw_img.c). Este probe mide las dos
 *  con la misma geometria y el mismo contenido, en el host (proxy x86, NO
 *  Cortex-A7): no es una medida de bateria.
 *
 *  Uso: wf_format_probe [reps]
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W 800
#define H 200

static uint64_t now_ns(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *) a, y = *(const double *) b;

    return (x > y) - (x < y);
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    (void) area;
    (void) px;
    lv_disp_flush_ready(drv);
}

static double bench(lv_obj_t *scr, lv_obj_t *img, int reps, double *p95) {
    double *t = malloc(sizeof(double) * (size_t) reps);

    for (int i = 0; i < reps; i++) {
        lv_obj_invalidate(img);
        uint64_t t0 = now_ns();

        lv_refr_now(NULL);
        t[i] = (double) (now_ns() - t0) / 1000000.0;
    }

    qsort(t, (size_t) reps, sizeof(double), cmp_double);
    double p50 = t[reps / 2];

    *p95 = t[(int) (reps * 0.95)];
    free(t);
    return p50;
}

static lv_obj_t *make_screen(void) {
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    return scr;
}

int main(int argc, char **argv) {
    int reps = 200;

    if (argc > 1) {
        reps = (int) strtol(argv[1], NULL, 10);
    }
    if (reps < 1) {
        reps = 1;
    }

    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t         disp_buf[W * 64];
    static lv_disp_drv_t      drv;

    lv_disp_draw_buf_init(&draw_buf, disp_buf, NULL, W * 64);
    lv_disp_drv_init(&drv);
    drv.draw_buf = &draw_buf;
    drv.flush_cb = flush_cb;
    drv.hor_res = W;
    drv.ver_res = 480;
    lv_disp_drv_register(&drv);

    /* TRUE_COLOR 32 bits: el frame del waterfall actual. */
    lv_img_dsc_t *tc = lv_img_buf_alloc(W, H, LV_IMG_CF_TRUE_COLOR);

    for (int y = 0; y < H; y++) {
        lv_color_t *row = (lv_color_t *) tc->data + (size_t) y * W;

        for (int x = 0; x < W; x++) {
            row[x].full = (uint32_t) ((x * 7 + y * 13) & 0xffffff) | 0xff000000u;
        }
    }

    /* INDEXED_8BIT: 1 byte por pixel + paleta de 256 lv_color32_t al inicio. */
    lv_img_dsc_t *idx = lv_img_buf_alloc(W, H, LV_IMG_CF_INDEXED_8BIT);
    lv_color32_t *pal = (lv_color32_t *) idx->data;
    uint8_t      *pix = (uint8_t *) idx->data + 4 * 256;

    for (int i = 0; i < 256; i++) {
        pal[i].ch.blue = (uint8_t) i;
        pal[i].ch.green = (uint8_t) (255 - i);
        pal[i].ch.red = (uint8_t) i;
        pal[i].ch.alpha = 0xff;
    }
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            pix[y * W + x] = (uint8_t) ((x * 7 + y * 13) & 0xff);
        }
    }

    lv_obj_t *scr = make_screen();
    lv_scr_load(scr);

    lv_obj_t *img_tc = lv_img_create(scr);
    lv_img_set_src(img_tc, tc);
    lv_obj_set_pos(img_tc, 0, 0);

    lv_obj_t *img_idx = lv_img_create(scr);
    lv_img_set_src(img_idx, idx);
    lv_obj_set_pos(img_idx, 0, 0);

    /* Cada imagen se mide sola (la otra oculta). */
    double p95;
    lv_obj_add_flag(img_idx, LV_OBJ_FLAG_HIDDEN);
    double tc50 = bench(scr, img_tc, reps, &p95);
    double tc95 = p95;
    lv_obj_add_flag(img_tc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img_idx, LV_OBJ_FLAG_HIDDEN);
    double idx50 = bench(scr, img_idx, reps, &p95);
    double idx95 = p95;

    printf("wf_format_probe %dx%d reps=%d (host x86, NO Cortex-A7)\n", W, H, reps);
    printf("  TRUE_COLOR 32b : p50=%.4f ms p95=%.4f ms\n", tc50, tc95);
    printf("  INDEXED_8BIT   : p50=%.4f ms p95=%.4f ms\n", idx50, idx95);
    printf("  ratio idx/tc   : p50=%.2fx p95=%.2fx\n", idx50 / tc50, idx95 / tc95);

    return 0;
}

/*
 * Fixtures deterministas del rasterizado incremental del waterfall (OPT-2,
 * issue #148).
 *
 * Compila el src/waterfall.c REAL contra LVGL 8.3.11 real y sustituye la radio
 * por stubs (tests/waterfall/stubs.c): params, cfg_cur/subjects, tema, paleta,
 * backlight y el scheduler. El oraculo de imagen es independiente del modulo:
 * reimplementa el mapeo fila->pixel del baseline y compara la ventana visible
 * que el anillo expone (waterfall_frame_pixels + waterfall_render_head).
 *
 * Uso: waterfall_test        (imprime PASS/FAIL y sale != 0 si algo falla)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"

#include "waterfall.h"
#include "params/params.h"
#include "meter.h"
#include "dsp.h"
#include "styles.h"
#include "scheduler.h"
#include "test_hooks.h"

#define WIDTH 800

static int checks;
static int failures;

#define CHECK(cond, ...)                                     \
    do {                                                     \
        checks++;                                            \
        if (!(cond)) {                                       \
            failures++;                                      \
            printf("  FAIL: ");                              \
            printf(__VA_ARGS__);                             \
            printf("\n");                                    \
        }                                                    \
    } while (0)

/* ---------------- display minimo ---------------- */

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    (void) area;
    (void) px;
    lv_disp_flush_ready(drv);
}

/* ---------------- oraculo (mapeo del baseline) ---------------- */

typedef struct {
    uint8_t  data[WATERFALL_NFFT];
    uint32_t width_hz;
    int32_t  center_freq;
} ref_row_t;

static lv_color_t palette_get(uint8_t id) {
    return (lv_color_t) wf_palette[id];
}

static void reference_render(const ref_row_t *row, uint32_t global_width, int32_t wf_center,
                             lv_color_t *out) {
    int32_t div = (int32_t) (global_width / WIDTH);

    if (div < 1) {
        div = 1;
    }

    lv_color_t black = lv_color_black();

    int32_t left = row->center_freq - (int32_t) row->width_hz / 2;
    int32_t right = left + (int32_t) row->width_hz;

    left -= wf_center;
    right -= wf_center;
    left = left / div + WIDTH / 2;
    right = right / div + WIDTH / 2;
    int32_t src_w = right - left;

    if ((right < 0) || (left > WIDTH) || (src_w <= 0)) {
        /* El baseline hace memset(...,0,...) para la fila entera fuera de vista:
         * 0x00000000, que NO es lv_color_black() (0xff000000 con alpha). */
        for (int32_t x = 0; x < WIDTH; x++) {
            out[x].full = 0;
        }
        return;
    }

    for (int32_t x = 0; x < WIDTH; x++) {
        uint16_t src_x = (uint16_t) ((x - left) * WATERFALL_NFFT / src_w);
        if ((src_x < 0) || (src_x >= WATERFALL_NFFT - 1)) {
            out[x] = black;
        } else {
            out[x] = palette_get(row->data[src_x]);
        }
    }
}

/* ---------------- estado del fixture ---------------- */

#define MAX_H 600

static ref_row_t ring[MAX_H];
static int       ring_h;
static int       ring_last;         /* ultimo id escrito */
static uint32_t  global_width;
static int32_t   wf_center;

static float row_buf[WATERFALL_NFFT];

static uint8_t id_from_db(float db, float min, float max) {
    float v = (db - min) / (max - min);

    if (v < 0.0f) {
        v = 0.0f;
    } else if (v > 1.0f) {
        v = 1.0f;
    }
    return (uint8_t) (v * 255);
}

static float db_for_id(uint8_t id, float min, float max) {
    return min + ((float) id / 255.0f) * (max - min);
}

/* Rearma el fixture: alto, y el anillo del oraculo con el estado inicial de la
 * cache del modulo (data 0, centro y ancho actuales). */
static void setup(int h) {
    waterfall_set_height(h);
    ring_h = h;
    ring_last = 0;
    global_width = (uint32_t) subject_get_int(cfg_cur.fft_width);
    wf_center = subject_get_int(cfg_cur.fg_freq);

    for (int i = 0; i < h && i < MAX_H; i++) {
        memset(&ring[i], 0, sizeof(ring[i]));
        ring[i].center_freq = wf_center;
        ring[i].width_hz = global_width;
    }
}

/* Feeds one constant row (all 512 samples equal) and drains the scheduler. */
static void feed_row_id(uint8_t id, bool tx, bool drain) {
    float min = tx ? S7 : S7;          /* auto min/max on => S7 / S9_40 */
    float max = S9_40;
    float db = db_for_id(id, min, max);

    for (int i = 0; i < WATERFALL_NFFT; i++) {
        row_buf[i] = db;
    }

    int next = (ring_last + 1) % ring_h;

    ring[next].center_freq = subject_get_int(cfg_cur.fg_freq);
    ring[next].width_hz = tx ? 48000 : (uint32_t) subject_get_int(cfg_cur.fft_width);
    memset(ring[next].data, id_from_db(db, min, max), WATERFALL_NFFT);

    waterfall_data(row_buf, WATERFALL_NFFT, tx);
    ring_last = next;

    if (drain) {
        scheduler_work();
    }
}

/* Compares the visible window the ring exposes against the reference. */
static int compare_window(void) {
    const lv_color_t *frame = waterfall_frame_pixels();

    if (frame == NULL) {
        return -1;
    }

    int head = waterfall_render_head();
    lv_color_t expected[WIDTH];
    int bad = 0;

    for (int j = 0; j < ring_h; j++) {
        int cache_id = ((ring_last - j) % ring_h + ring_h) % ring_h;

        reference_render(&ring[cache_id], global_width, wf_center, expected);

        const lv_color_t *win = frame + (size_t) (((head + j) % ring_h) * WIDTH);

        for (int x = 0; x < WIDTH; x++) {
            if (win[x].full != expected[x].full) {
                if (bad < 3) {
                    printf("  row %d x %d: frame 0x%08x != ref 0x%08x\n",
                           j, x, (unsigned) win[x].full, (unsigned) expected[x].full);
                }
                bad++;
            }
        }
    }

    return bad;
}

/* ---------------- paleta de prueba ---------------- */

static uint32_t palette_fn(uint8_t i) {
    return (uint32_t) (i << 16) | (uint32_t) ((255 - i) << 8) | (uint32_t) i;
}

static void reset_palette(void) {
    test_palette_fill(palette_fn);
}

/* ---------------- fixtures ---------------- */

/* 20 vueltas de anillo: H=8, 160 filas, comparando la ventana fila a fila y
 * asertando el offset (render_head) en cada paso. */
static void fixture_ring_turns(void) {
    printf("fixture: 20 vueltas de anillo (H=8, 160 filas)\n");

    setup(8);

    /* Primera fila: render completo (head queda 0). Desde aca se mide el
     * camino incremental. */
    feed_row_id(0x5a, false, true);

    uint64_t px0 = waterfall_rasterized_px();
    uint32_t rebuilds0 = waterfall_full_rebuild_count();
    int bad_total = 0;

    for (int k = 1; k <= 160; k++) {
        feed_row_id((uint8_t) ((k * 13 + 1) & 0xff), false, true);

        int expected_head = (8 - (k % 8)) % 8;
        CHECK(waterfall_render_head() == expected_head,
              "k=%d: render_head=%d, esperado %d", k, waterfall_render_head(), expected_head);

        int bad = compare_window();
        bad_total += (bad > 0) ? 1 : 0;
        if (bad > 0 && bad_total <= 2) {
            printf("  k=%d: %d px fuera de lugar\n", k, bad);
        }
    }

    CHECK(bad_total == 0, "%d de 160 ventanas no coinciden con el oraculo", bad_total);
    /* El anillo nunca reconstruye: 160 filas -> 160*800 px, 0 rebuilds. */
    CHECK(waterfall_rasterized_px() - px0 == 160ull * WIDTH,
          "rasterized_px=%llu, esperado %llu",
          (unsigned long long) (waterfall_rasterized_px() - px0),
          (unsigned long long) (160ull * WIDTH));
    CHECK(waterfall_full_rebuild_count() - rebuilds0 == 0, "el anillo reconstruyo de mas");
}

/* Scroll con cambio de frecuencia central durante el barrido. */
static void fixture_center_change(void) {
    printf("fixture: cambio de frecuencia central durante el scroll\n");

    subject_set_int(cfg_cur.fg_freq, 14074000);
    subject_set_int(cfg_cur.fft_width, 100000);
    setup(16);

    uint32_t rebuilds0 = waterfall_full_rebuild_count();

    for (int k = 0; k < 40; k++) {
        /* Cada 8 filas el operador mueve el VFO: la vista sigue por el camino
         * real (do_scroll_cb en DRAW_POST_END del img) y exige reconstruccion
         * completa. */
        if (k > 0 && (k % 8) == 0) {
            subject_set_int(cfg_cur.fg_freq, 14074000 + k * 500);
            lv_obj_invalidate(lv_scr_act());   /* redibuja el img -> DRAW_POST_END */
            lv_refr_now(NULL);                 /* dispara do_scroll_cb */
            scheduler_work();                  /* corre el refresh pedido */
            wf_center = subject_get_int(cfg_cur.fg_freq);
        }
        feed_row_id((uint8_t) ((k * 7 + 3) & 0xff), false, true);
    }

    CHECK(waterfall_full_rebuild_count() - rebuilds0 >= 4,
          "el cambio de centro no reconstruyo (rebuilds=%u)",
          waterfall_full_rebuild_count() - rebuilds0);
    CHECK(compare_window() == 0, "la ventana tras el scroll no coincide con el oraculo");
}

/* Cambios de zoom (100 kHz -> estrecho -> 100 kHz). */
static void fixture_zoom(void) {
    printf("fixture: cambios de zoom (100 kHz -> 30 kHz -> 100 kHz)\n");

    subject_set_int(cfg_cur.fft_width, 100000);
    setup(24);

    for (int k = 0; k < 6; k++) {
        feed_row_id((uint8_t) (k + 10), false, true);
    }

    const uint32_t widths[] = { 30000, 100000, 50000 };
    for (size_t w = 0; w < sizeof(widths) / sizeof(widths[0]); w++) {
        subject_set_int(cfg_cur.fft_width, (int32_t) widths[w]);
        global_width = widths[w];
        feed_row_id((uint8_t) (20 + w), false, true);
        CHECK(compare_window() == 0, "zoom %u: la ventana no coincide", widths[w]);
    }
}

/* Cambio de paleta/tema a mitad de corrida. */
static void fixture_palette(void) {
    printf("fixture: cambio de paleta\n");

    reset_palette();
    subject_set_int(cfg_cur.fft_width, 100000);
    setup(12);

    for (int k = 0; k < 6; k++) {
        feed_row_id((uint8_t) (k + 30), false, true);
    }
    CHECK(compare_window() == 0, "antes del cambio de paleta");

    /* Nueva paleta: el proximo refresh tiene que reconstruir. */
    for (int i = 0; i < 256; i++) {
        test_palette_set((uint8_t) i, (uint32_t) (0x00ff00u ^ (uint32_t) (i * 0x010101u)));
    }
    feed_row_id(77, false, true);
    CHECK(compare_window() == 0, "tras el cambio de paleta la ventana no coincide");
}

/* RX -> TX: la fila TX viaja con su propio ancho (48000) y no exige rebuild. */
static void fixture_rx_tx(void) {
    printf("fixture: RX <-> TX\n");

    reset_palette();
    subject_set_int(cfg_cur.fft_width, 100000);
    setup(10);

    for (int k = 0; k < 5; k++) {
        feed_row_id((uint8_t) (k + 40), false, true);
    }
    for (int k = 0; k < 5; k++) {
        feed_row_id((uint8_t) (k + 60), true, true);   /* TX */
    }
    CHECK(compare_window() == 0, "la ventana RX/TX no coincide con el oraculo");
}

/* ECO (refresh_period=4) y pantalla OFF/ON: ninguna fila se pierde. */
static void fixture_eco_display(void) {
    printf("fixture: ECO (1/4) y pantalla OFF/ON\n");

    reset_palette();
    subject_set_int(cfg_cur.fft_width, 100000);
    setup(16);

    /* ECO: 4 filas, solo 1 refresco corre; el resto queda pendiente y el
     * siguiente refresco las muestra todas. */
    waterfall_refresh_period_set(4);
    for (int k = 0; k < 4; k++) {
        feed_row_id((uint8_t) (k + 1), false, false);   /* sin drenar */
    }
    scheduler_work();                                   /* 3 se saltean, 1 corre */
    /* El scheduler corrio los 4 refresh; con period=4 solo 1 rasterizo (las 4
     * filas pendientes), los otros 3 volvieron temprano. */
    CHECK(compare_window() == 0, "ECO: la ventana no coincide (filas perdidas)");
    waterfall_refresh_reset();

    /* Pantalla apagada durante N filas: no se rasteriza; al encender, el
     * proximo refresh muestra todas las que llegaron. */
    test_backlight_set(false);
    uint64_t px0 = waterfall_rasterized_px();
    for (int k = 0; k < 8; k++) {
        feed_row_id((uint8_t) (k + 20), false, false);
    }
    scheduler_work();
    CHECK(waterfall_rasterized_px() - px0 == 0,
          "pantalla apagada: rasterizo %llu px",
          (unsigned long long) (waterfall_rasterized_px() - px0));

    test_backlight_set(true);
    feed_row_id(99, false, true);   /* el wake muestra todas las pendientes */
    CHECK(compare_window() == 0, "wake: la ventana no coincide con el historial");
}

/* Limites: alto 0/1/512/513, >512 muestras, ancho estrecho y reserva fallida. */
static void fixture_limits(void) {
    printf("fixture: limites\n");

    reset_palette();
    subject_set_int(cfg_cur.fft_width, 100000);
    wf_center = subject_get_int(cfg_cur.fg_freq);

    /* Alto 0: inerte, sin crash. */
    waterfall_set_height(0);
    CHECK(waterfall_frame_pixels() == NULL, "alto 0: hay frame");
    for (int i = 0; i < WATERFALL_NFFT; i++) row_buf[i] = -60.0f;
    waterfall_data(row_buf, WATERFALL_NFFT, false);   /* no debe tocar nada */
    scheduler_work();
    CHECK(waterfall_frame_pixels() == NULL, "alto 0: se creo un frame");
    CHECK(waterfall_frame_height() == 0, "alto 0: height=%d", waterfall_frame_height());

    /* Alto 1 y 512. */
    setup(1);
    CHECK(waterfall_frame_height() == 1, "alto 1: height=%d", waterfall_frame_height());
    feed_row_id(123, false, true);
    CHECK(compare_window() == 0, "alto 1: ventana");

    setup(512);
    CHECK(waterfall_frame_height() == 512, "alto 512: height=%d", waterfall_frame_height());

    /* size > WATERFALL_NFFT: se recorta a la capacidad real del buffer (512),
     * asi que no lee mas alla de row_buf (ASan lo cubre: el buffer es de 512 y
     * el test pide 1024). */
    for (int i = 0; i < WATERFALL_NFFT; i++) row_buf[i] = -50.0f;
    waterfall_data(row_buf, 1024, false);
    scheduler_work();

    /* Ancho en Hz estrecho: divisor 0 en el baseline, acotado aca. */
    subject_set_int(cfg_cur.fft_width, 400);
    setup(4);
    feed_row_id(50, false, true);
    CHECK(compare_window() == 0, "ancho estrecho: ventana");

    subject_set_int(cfg_cur.fft_width, 100000);

    /* Reserva fallida (hook): el frame no se puede reservar y el modulo queda
     * inerte en vez de crashear. */
    test_fail_img_alloc = true;
    waterfall_set_height(8);
    CHECK(waterfall_frame_pixels() == NULL, "reserva fallida: quedo un frame");
    CHECK(waterfall_frame_height() == 0, "reserva fallida: height=%d", waterfall_frame_height());
    waterfall_data(row_buf, WATERFALL_NFFT, false);
    scheduler_work();
    CHECK(waterfall_frame_pixels() == NULL, "reserva fallida: se creo un frame");
}

/* Instrumento: una fila nueva en reposo cuesta WIDTH px, no WIDTH*height. */
static void fixture_instrument(void) {
    printf("fixture: instrumento de rasterizado\n");

    reset_palette();
    subject_set_int(cfg_cur.fft_width, 100000);
    setup(100);

    uint64_t before = waterfall_rasterized_px();

    feed_row_id(1, false, true);   /* primer refresh: reconstruccion completa */
    uint64_t after_first = waterfall_rasterized_px();
    CHECK(after_first - before == 100ull * WIDTH,
          "primer render: %llu px, esperado %llu",
          (unsigned long long) (after_first - before),
          (unsigned long long) (100ull * WIDTH));

    for (int k = 0; k < 9; k++) {
        feed_row_id((uint8_t) (k + 2), false, true);
    }
    CHECK(waterfall_rasterized_px() - after_first == 9ull * WIDTH,
          "9 filas nuevas: %llu px, esperado %llu",
          (unsigned long long) (waterfall_rasterized_px() - after_first),
          (unsigned long long) (9ull * WIDTH));
}

int main(void) {
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t         disp_buf[WIDTH * 64];
    static lv_disp_drv_t      drv;

    lv_disp_draw_buf_init(&draw_buf, disp_buf, NULL, WIDTH * 64);
    lv_disp_drv_init(&drv);
    drv.draw_buf = &draw_buf;
    drv.flush_cb = flush_cb;
    drv.hor_res = WIDTH;
    drv.ver_res = 480;
    lv_disp_drv_register(&drv);

    /* Los subjects que cfg_cur referencia, como los publica la radio. */
    cfg_cur.fg_freq = test_subject_create(14074000);
    cfg_cur.fft_width = test_subject_create(100000);

    static cfg_band_t band;
    band.grid.min.val = test_subject_create(S7);
    band.grid.max.val = test_subject_create(S9_40);
    cfg_cur.band = &band;

    params.waterfall_smooth_scroll.x = false;
    params.waterfall_center_line.x = true;
    params.waterfall_auto_min.x = true;
    params.waterfall_auto_max.x = true;

    reset_palette();

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *wf = waterfall_init(scr);
    (void) wf;

    /* La pantalla tiene que ser la activa: `do_scroll_cb` corre en el
     * DRAW_POST_END del img y necesita que el refresco lo dibuje. */
    lv_scr_load(scr);

    fixture_ring_turns();
    fixture_center_change();
    fixture_zoom();
    fixture_palette();
    fixture_rx_tx();
    fixture_eco_display();
    fixture_limits();
    fixture_instrument();

    printf("%s: %d checks, %d fallas\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}

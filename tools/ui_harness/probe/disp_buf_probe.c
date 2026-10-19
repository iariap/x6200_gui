/*
 *  Draw-buffer / software-rotation probe for the X6200 GUI (issue #147).
 *
 *  Why it exists: the firmware's draw buffer is a static array sized in
 *  *pixels*, and the display runs with `sw_rotate = 1` and `ROT_90` (logical
 *  800x480, physical 480x800). Sizing that buffer down is only safe if LVGL
 *  8.3.11's partial-buffer rendering produces exactly the same physical
 *  framebuffer for every buffer size. The host UI harness cannot answer that:
 *  it draws landscape into an 800x480 framebuffer with no rotation and a
 *  full-screen buffer. This probe links the *real* LVGL 8.3.11 from the
 *  submodule with the firmware's display configuration and reconstructs the
 *  physical framebuffer from the flush callbacks, so the question becomes a
 *  byte comparison.
 *
 *  It is a measurement instrument, not part of the shipped firmware: no radio,
 *  no network, no GPU. It runs one draw-buffer size per process (`--px`), walks
 *  a fixed sequence of scenes/phases, and writes:
 *    - `<prefix>.fb`  : the physical framebuffer after every phase, raw
 *                       uint32 row-major, FW_HOR*FW_VER values per phase;
 *    - `<prefix>.json`: per-phase flush contract metrics.
 *  A driver (tests/check_drawbuf_partial.sh) compares the `.fb` of every size
 *  against the reference and checks the JSON invariants.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "lvgl/lvgl.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* Firmware display geometry: physical panel is portrait, the GUI is landscape
 * and LVGL rotates it in software (src/main.c). */
#define FW_HOR 480
#define FW_VER 800
#define LOG_W  800
#define LOG_H  480

/* The current firmware buffer: 800 * 480 * 4 *pixels* (4x one screen). */
#define PX_CURRENT (800 * 480 * 4)
/* One full screen: 800 * 480 pixels. */
#define PX_FULL    (800 * 480)

typedef struct {
    uint32_t flushes;       /* flush callbacks in the phase */
    uint32_t last_count;    /* lv_disp_flush_is_last() true, must be 1 per cycle */
    uint32_t out_of_area;   /* flush rect outside the physical panel */
    uint64_t flushed_px;    /* pixels handed to the flush callback */
    uint32_t covered_px;    /* distinct physical pixels written */
    uint32_t dup_px;        /* physical pixels written more than once */
} phase_metrics_t;

#define MAX_PHASES 16

static uint32_t       *fb;             /* reconstructed physical framebuffer */
static uint16_t       *seen;           /* writes per physical pixel, current phase */
static phase_metrics_t metrics[MAX_PHASES];
static int             phase_count;
static uint32_t        cur_flushes, cur_last, cur_out;
static uint64_t        cur_flushed;

static void phase_reset(void)
{
    memset(seen, 0, (size_t) FW_HOR * FW_VER * sizeof(uint16_t));
    cur_flushes = cur_last = cur_out = 0;
    cur_flushed = 0;
}

static void phase_record(void)
{
    phase_metrics_t *m = &metrics[phase_count++];
    uint32_t covered = 0, dup = 0;

    for (size_t i = 0; i < (size_t) FW_HOR * FW_VER; i++) {
        if (seen[i] > 0) covered++;
        if (seen[i] > 1) dup++;
    }

    m->flushes     = cur_flushes;
    m->last_count  = cur_last;
    m->out_of_area = cur_out;
    m->flushed_px  = cur_flushed;
    m->covered_px  = covered;
    m->dup_px      = dup;
}

/* The display flush callback: reconstruct the physical framebuffer from the
 * (already rotated) area LVGL hands over, exactly like fbdev_flush would write
 * it to /dev/fb0, and account the flush contract. */
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px)
{
    bool last = lv_disp_flush_is_last(drv);
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    if (area->x1 < 0 || area->y1 < 0 || area->x2 >= FW_HOR || area->y2 >= FW_VER) {
        cur_out++;
    }

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            int32_t X = area->x1 + x;
            int32_t Y = area->y1 + y;

            if (X >= 0 && Y >= 0 && X < FW_HOR && Y < FW_VER) {
                uint32_t v = px->full;

                fb[Y * FW_HOR + X] = v;
                seen[Y * FW_HOR + X]++;
            }
            px++;
        }
    }

    cur_flushes++;
    cur_flushed += (uint64_t) w * (uint64_t) h;
    if (last) cur_last++;

    lv_disp_flush_ready(drv);
}

static lv_obj_t *make_rect(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                           uint32_t color, lv_opa_t opa, int32_t radius)
{
    lv_obj_t *o = lv_obj_create(parent);

    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_set_style_radius(o, radius, 0);
    return o;
}

/* Screen A: a handful of overlapping, unaligned and edge-touching rectangles
 * plus a label (glyph rendering) so a rotation/band-split bug has something to
 * corrupt. */
static lv_obj_t *build_screen_a(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    make_rect(scr, 10, 10, 300, 200, 0xE04030, LV_OPA_COVER, 8);
    make_rect(scr, 250, 150, 400, 300, 0x3070E0, LV_OPA_60, 0);
    make_rect(scr, 3, 5, 17, 23, 0x30E070, LV_OPA_COVER, 2);   /* unaligned */
    make_rect(scr, 795, 470, 5, 10, 0xF0F0F0, LV_OPA_COVER, 0); /* bottom-right edge */
    make_rect(scr, 0, 0, 1, 1, 0xFFFFFF, LV_OPA_COVER, 0);      /* top-left corner */

    lv_obj_t *label = lv_label_create(scr);

    lv_obj_set_style_text_color(label, lv_color_hex(0xF0F0F0), 0);
    lv_label_set_text(label, "X6200 draw buffer 0123456789 ABCDEF");
    lv_obj_set_pos(label, 40, 400);

    return scr;
}

/* Screen B: different geometry, so a screen change has to repaint everything. */
static lv_obj_t *build_screen_b(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x201010), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    make_rect(scr, 0, 0, 400, 240, 0xE0C030, LV_OPA_COVER, 0);
    make_rect(scr, 399, 239, 401, 241, 0x40C0E0, LV_OPA_50, 0);
    make_rect(scr, 100, 300, 600, 180, 0xC040E0, LV_OPA_COVER, 12);

    lv_obj_t *label = lv_label_create(scr);

    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(label, "screen B");
    lv_obj_set_pos(label, 20, 20);

    return scr;
}

static void snapshot(FILE *out)
{
    fwrite(fb, sizeof(uint32_t), (size_t) FW_HOR * FW_VER, out);
}

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *) a, y = *(const double *) b;

    return (x > y) - (x < y);
}

/* Times `reps` redraws of `area` on the host and prints p50/p95 and the flushes
 * per redraw. This is an x86 proxy (rule: never a Cortex-A7 or battery
 * measurement); it exists to check the H4 budget of the buffer-size experiment.
 * `area == NULL` invalidates the whole screen. */
static void bench_redraw(lv_disp_t *disp, lv_obj_t *scr, const lv_area_t *area,
                         int reps, const char *label)
{
    double *t = malloc(sizeof(double) * (size_t) reps);
    uint32_t flush_lo = 0xffffffffu, flush_hi = 0;

    if (!t) return;

    for (int i = 0; i < reps; i++) {
        uint32_t f0 = cur_flushes;
        uint64_t t0;

        if (area) lv_obj_invalidate_area(scr, area);
        else      lv_obj_invalidate(scr);

        t0 = now_ns();
        lv_refr_now(disp);
        t[i] = (double) (now_ns() - t0) / 1000000.0;

        uint32_t f = cur_flushes - f0;

        if (f < flush_lo) flush_lo = f;
        if (f > flush_hi) flush_hi = f;
    }

    qsort(t, (size_t) reps, sizeof(double), cmp_double);
    printf("bench_%s reps=%d p50=%.3f ms p95=%.3f ms min=%.3f max=%.3f flushes=%u..%u\n",
           label, reps, t[reps / 2], t[(int) (reps * 0.95)], t[0], t[reps - 1],
           flush_lo, flush_hi);
    free(t);
}

int main(int argc, char **argv)
{
    uint32_t px = PX_CURRENT;
    const char *prefix = "probe";
    int bench_reps = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--px") == 0 && i + 1 < argc) {
            px = (uint32_t) strtoul(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            prefix = argv[++i];
        }
        else if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) {
            bench_reps = (int) strtol(argv[++i], NULL, 10);
        }
        else {
            fprintf(stderr, "uso: %s [--px <size_in_px_cnt>] [--out <prefix>] [--bench <reps>]\n", argv[0]);
            return 2;
        }
    }

    if (px < LOG_W) {
        fprintf(stderr, "px=%u: un buffer parcial necesita al menos una linea (%d px)\n", px, LOG_W);
        return 2;
    }

    lv_init();

    fb   = calloc((size_t) FW_HOR * FW_VER, sizeof(uint32_t));
    seen = calloc((size_t) FW_HOR * FW_VER, sizeof(uint16_t));

    /* mmap so mincore() can tell reserved (mapped) from resident (touched)
     * pages of the draw buffer: the firmware's buffer is a static BSS array,
     * and the kernel only backs the pages LVGL actually writes. */
    size_t buf_bytes = (size_t) px * sizeof(lv_color_t);
    size_t buf_map   = (buf_bytes + (size_t) getpagesize() - 1) / (size_t) getpagesize() * (size_t) getpagesize();
    lv_color_t *buf  = mmap(NULL, buf_map, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (!fb || !seen || buf == MAP_FAILED) {
        fprintf(stderr, "sin memoria\n");
        return 2;
    }

    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t      drv;

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, px);
    lv_disp_drv_init(&drv);
    drv.draw_buf  = &draw_buf;
    drv.flush_cb  = flush_cb;
    drv.hor_res   = FW_HOR;
    drv.ver_res   = FW_VER;
    drv.sw_rotate = 1;
    drv.rotated   = LV_DISP_ROT_90;
    lv_disp_drv_register(&drv);

    lv_disp_t *disp = lv_disp_get_default();

    lv_disp_set_bg_color(disp, lv_color_black());
    lv_disp_set_bg_opa(disp, LV_OPA_COVER);

    lv_obj_t *scr_a = build_screen_a();
    lv_obj_t *scr_b = build_screen_b();

    char path[512];
    snprintf(path, sizeof(path), "%s.fb", prefix);

    FILE *out = fopen(path, "wb");

    if (!out) {
        fprintf(stderr, "no puedo abrir %s\n", path);
        return 2;
    }

    /* Phase 0: full-screen invalidation of screen A. */
    lv_scr_load_anim(scr_a, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    phase_reset();
    lv_obj_invalidate(scr_a);
    lv_refr_now(disp);
    snapshot(out);
    phase_record();

    /* Phase 1: unaligned sub-area (a widget change + an odd rectangle). */
    lv_obj_set_style_bg_color(lv_obj_get_child(scr_a, 0), lv_color_hex(0x20A0A0), 0);
    phase_reset();
    {
        lv_area_t a = { 5, 3, 797, 474 };

        lv_obj_invalidate_area(scr_a, &a);
    }
    lv_refr_now(disp);
    snapshot(out);
    phase_record();

    /* Phase 2: three disjoint rectangles in one cycle: 1-px columns at both
     * edges plus a band that crosses a 64-line boundary. */
    phase_reset();
    {
        lv_area_t a1 = { 0, 0, 0, 479 };
        lv_area_t a2 = { 799, 0, 799, 479 };
        lv_area_t a3 = { 300, 58, 420, 70 };

        lv_obj_invalidate_area(scr_a, &a1);
        lv_obj_invalidate_area(scr_a, &a2);
        lv_obj_invalidate_area(scr_a, &a3);
    }
    lv_refr_now(disp);
    snapshot(out);
    phase_record();

    /* Phase 3: a rectangle crossing the 32/64/128-line boundaries. */
    phase_reset();
    {
        lv_area_t a = { 100, 118, 700, 133 };

        lv_obj_invalidate_area(scr_a, &a);
    }
    lv_refr_now(disp);
    snapshot(out);
    phase_record();

    /* Phase 4: a 1-px row. */
    phase_reset();
    {
        lv_area_t a = { 0, 240, 799, 240 };

        lv_obj_invalidate_area(scr_a, &a);
    }
    lv_refr_now(disp);
    snapshot(out);
    phase_record();

    /* Phase 5: screen change to B (full repaint of a different screen). */
    phase_reset();
    lv_scr_load_anim(scr_b, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    lv_refr_now(disp);
    snapshot(out);
    phase_record();

    /* Phase 6: back to A (full repaint again). */
    phase_reset();
    lv_scr_load_anim(scr_a, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    lv_refr_now(disp);
    snapshot(out);
    phase_record();

    /* Phase 7: full-screen invalidation after moving a widget. */
    phase_reset();
    lv_obj_set_pos(lv_obj_get_child(scr_a, 1), 200, 100);
    lv_obj_invalidate(scr_a);
    lv_refr_now(disp);
    snapshot(out);
    phase_record();

    fclose(out);

    if (bench_reps > 0) {
        lv_area_t spectrum  = { 0, 0, LOG_W - 1, 164 };    /* spectrum strip 800x165 */
        lv_area_t waterfall = { 0, 235, LOG_W - 1, 479 };  /* waterfall strip 800x245 */
        lv_area_t band      = { 100, 118, 700, 133 };      /* 601x16, crosses band lines */

        bench_redraw(disp, scr_a, NULL, bench_reps, "full_redraw");
        bench_redraw(disp, scr_a, &spectrum, bench_reps, "spectrum_800x165");
        bench_redraw(disp, scr_a, &waterfall, bench_reps, "waterfall_800x245");
        bench_redraw(disp, scr_a, &band, bench_reps, "band_601x16");
    }

    /* Reserved (mapped) vs resident (touched) pages of the draw buffer, after
     * the whole scene sequence: this is what the kernel would back on the
     * radio, where the buffer is a static BSS array. */
    {
        long ps = sysconf(_SC_PAGESIZE);
        size_t pages = buf_map / (size_t) ps;
        unsigned char *vec = malloc(pages);

        if (vec && mincore(buf, buf_map, vec) == 0) {
            size_t resident = 0;

            for (size_t i = 0; i < pages; i++) {
                if (vec[i] & 1) resident++;
            }
            printf("draw_buffer reserved_bytes=%zu resident_bytes=%zu touched_pct=%.1f\n",
                   buf_bytes, resident * (size_t) ps, 100.0 * (double) resident / (double) pages);
        }
        free(vec);
    }

    snprintf(path, sizeof(path), "%s.json", prefix);
    out = fopen(path, "wb");
    if (!out) {
        fprintf(stderr, "no puedo abrir %s\n", path);
        return 2;
    }
    fprintf(out, "{\"px\":%u,\"lines\":%u,\"phases\":[", px, px / LOG_W);
    for (int i = 0; i < phase_count; i++) {
        fprintf(out,
                "%s{\"i\":%d,\"flushes\":%u,\"last\":%u,\"out_of_area\":%u,"
                "\"flushed_px\":%llu,\"covered_px\":%u,\"dup_px\":%u}",
                i ? "," : "", i, metrics[i].flushes, metrics[i].last_count,
                metrics[i].out_of_area, (unsigned long long) metrics[i].flushed_px,
                metrics[i].covered_px, metrics[i].dup_px);
    }
    fprintf(out, "]}\n");
    fclose(out);

    printf("px=%u lines=%u phases=%d\n", px, px / LOG_W, phase_count);
    for (int i = 0; i < phase_count; i++) {
        printf("  phase %d: flushes=%u last=%u out=%u flushed_px=%llu\n", i,
               metrics[i].flushes, metrics[i].last_count, metrics[i].out_of_area,
               (unsigned long long) metrics[i].flushed_px);
    }

    return 0;
}

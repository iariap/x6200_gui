/*
 *  Real-clock render benchmark for the host UI harness (see bench_metrics.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "bench_metrics.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "display.h"

#define BENCH_MAX_FRAMES 512
#define BENCH_MAX_FLUSHES 8192

static bool   enabled;
static bool   frame_open;

static bench_frame_t frames[BENCH_MAX_FRAMES];
static size_t        frame_count;

static double            flush_ms_accum;
static struct timespec   flush_started;
static uint32_t          frame_flushes;
static uint64_t          frame_redrawn;

/* Flushed rectangles of the current frame, rasterised after the render
 * finishes so the union is never computed inside the measured window. */
static int32_t rect_x[BENCH_MAX_FLUSHES];
static int32_t rect_y[BENCH_MAX_FLUSHES];
static int32_t rect_w[BENCH_MAX_FLUSHES];
static int32_t rect_h[BENCH_MAX_FLUSHES];
static size_t  rect_count;

static uint8_t coverage[HARNESS_SCREEN_W * HARNESS_SCREEN_H];

uint64_t bench_now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

void bench_enable(bool on)
{
    enabled = on;
}

bool bench_enabled(void)
{
    return enabled;
}

void bench_flush_begin(void)
{
    if (!enabled || !frame_open) {
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &flush_started);
}

void bench_flush_end(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    if (!enabled || !frame_open || x1 < x0 || y1 < y0) {
        return;
    }

    uint64_t elapsed = bench_now_ns() - ((uint64_t) flush_started.tv_sec * 1000000000ull +
                                         (uint64_t) flush_started.tv_nsec);
    int32_t width = x1 - x0 + 1;
    int32_t height = y1 - y0 + 1;

    flush_ms_accum += (double) elapsed / 1000000.0;
    frame_flushes++;
    frame_redrawn += (uint64_t) width * (uint64_t) height;

    if (rect_count < BENCH_MAX_FLUSHES) {
        rect_x[rect_count] = x0;
        rect_y[rect_count] = y0;
        rect_w[rect_count] = width;
        rect_h[rect_count] = height;
        rect_count++;
    }
}

void bench_frame_begin(void)
{
    if (!enabled) {
        return;
    }
    frame_open = true;
    flush_ms_accum = 0.0;
    frame_flushes = 0;
    frame_redrawn = 0;
    rect_count = 0;
    memset(coverage, 0, sizeof(coverage));
}

void bench_frame_end(double render_ms)
{
    if (!enabled || !frame_open) {
        return;
    }

    /* Distinct pixels: rasterise the flushed rectangles (all of them are the
     * full framebuffer here, but the union stays correct if a future flush
     * covers partial areas). */
    uint64_t unique = 0;

    for (size_t i = 0; i < rect_count; i++) {
        int32_t x0 = rect_x[i];
        int32_t y0 = rect_y[i];
        int32_t x1 = x0 + rect_w[i];
        int32_t y1 = y0 + rect_h[i];

        if (x1 > HARNESS_SCREEN_W) { x1 = HARNESS_SCREEN_W; }
        if (y1 > HARNESS_SCREEN_H) { y1 = HARNESS_SCREEN_H; }
        for (int32_t y = y0; y < y1; y++) {
            for (int32_t x = x0; x < x1; x++) {
                size_t idx = (size_t) y * HARNESS_SCREEN_W + (size_t) x;

                if (!coverage[idx]) {
                    coverage[idx] = 1;
                    unique++;
                }
            }
        }
    }

    if (frame_count < BENCH_MAX_FRAMES) {
        double draw_ms = render_ms - flush_ms_accum;

        if (draw_ms < 0.0) {
            draw_ms = 0.0;
        }
        frames[frame_count].render_ms = render_ms;
        frames[frame_count].flush_ms = flush_ms_accum;
        frames[frame_count].draw_ms = draw_ms;
        frames[frame_count].flushes = frame_flushes;
        frames[frame_count].redrawn_area_px =
            frame_redrawn > UINT32_MAX ? UINT32_MAX : (uint32_t) frame_redrawn;
        frames[frame_count].unique_redrawn_px = unique > UINT32_MAX ? UINT32_MAX : (uint32_t) unique;
        frame_count++;
    }
    frame_open = false;
}

size_t bench_frame_count(void)
{
    return frame_count;
}

const bench_frame_t *bench_frame_at(size_t index)
{
    if (index >= frame_count) {
        return NULL;
    }
    return &frames[index];
}

/* --- process memory ------------------------------------------------------- */

static uint64_t status_field(const char *key)
{
    FILE *f = fopen("/proc/self/status", "r");

    if (!f) {
        return 0;
    }

    char line[256];
    uint64_t value = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(key);

        if (strncmp(line, key, len) == 0) {
            unsigned long long kb = 0;

            if (sscanf(line + len, ": %llu", &kb) == 1) {
                value = (uint64_t) kb * 1024ull;
            }
            break;
        }
    }
    fclose(f);
    return value;
}

static uint64_t smaps_rollup_pss(void)
{
    FILE *f = fopen("/proc/self/smaps_rollup", "r");

    if (!f) {
        return 0;
    }

    char line[256];
    uint64_t value = 0;

    while (fgets(line, sizeof(line), f)) {
        unsigned long long kb = 0;

        if (sscanf(line, "Pss: %llu", &kb) == 1) {
            value = (uint64_t) kb * 1024ull;
            break;
        }
    }
    fclose(f);
    return value;
}

bench_memory_t bench_read_memory(void)
{
    bench_memory_t memory;

    memory.rss_bytes = status_field("VmRSS");
    memory.peak_rss_bytes = status_field("VmHWM");
    memory.peak_vm_bytes = status_field("VmPeak");
    memory.pss_bytes = smaps_rollup_pss();
    return memory;
}

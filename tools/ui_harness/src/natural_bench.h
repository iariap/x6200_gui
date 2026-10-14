/*
 *  Natural-invalidation bench for the host UI harness.
 *
 *  The forced bench (bench_metrics.h, `--bench`) measures a full-screen repaint
 *  the harness fabricates itself. This mode measures the *natural* work: the
 *  invalidation the real components do on their own timers, with the
 *  spectrum/waterfall magnitudes driven frame by frame by the deterministic
 *  generator (signal_gen.c). One natural frame is one LVGL display refresh
 *  period of virtual time. See README.md for the protocol and the JSON.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lvgl/lvgl.h"

#include "scenario.h"

/* One natural frame is one LVGL display refresh period (LV_DISP_DEF_REFR_PERIOD,
 * 30 ms): the granularity at which LVGL decides to refresh on its own. The
 * clock advances in slices of this size, and the data drive is checked once per
 * slice (the same 10 ms the plain capture uses). */
#define NATURAL_BENCH_SLICE_MS 10

/* Frames a natural run does when neither the fixture nor the CLI say. */
#define NATURAL_BENCH_FRAMES_DEFAULT 40

/* Invalidation rectangles one frame can carry; the frames of the main screen
 * have at most one per component that changed (spectrum, waterfall, clock). A
 * frame with more sets `rects_complete` to false: the sums stay complete, only
 * the reported rectangles are capped. */
#define NATURAL_BENCH_MAX_RECTS 8

typedef struct {
    uint64_t  virtual_end_ms;    /* harness' virtual clock at the end of the frame */
    uint32_t  refresh_cycles;    /* refresh cycles LVGL completed in the frame */
    uint32_t  invalidated_px;    /* sum of the invalidation rectangles it collected */
    uint32_t  invalidated_rects; /* how many unjoined rectangles those were */
    uint32_t  joined_px;         /* area LVGL merged away (joined rectangles) */
    uint32_t  flushed_px;        /* sum of the rectangles the driver was given */
    uint32_t  flushes;           /* flush callback invocations */
    bool      rects_complete;    /* the reported rectangles are all of them */
    lv_area_t rects[NATURAL_BENCH_MAX_RECTS];
} natural_frame_t;

/* Enables the instrumentation. When disabled every hook is a no-op, so the
 * normal (PNG) mode and the forced bench are not touched. */
void natural_bench_enable(bool on);
bool natural_bench_enabled(void);

/* Whether the run drives the spectrum/waterfall data frame by frame. With it
 * off the only natural invalidation left is the clock's: the delimitation
 * control of tests/check_harness_natural.sh. */
void natural_bench_set_drive(bool on);
bool natural_bench_drive(void);

/* Runs a natural bench of `frames` frames on the scenario's seed, advancing the
 * virtual clock (no sleeps) and collecting the per-frame metrics. */
void natural_bench_run(const scenario_t *scenario, int frames);

/* Flush callback hook: call with the flushed rectangle and the driver whose
 * draw buffer marks the end of a refresh cycle. Records the cycle once. */
void natural_bench_flush(const lv_area_t *area, const lv_disp_drv_t *drv);

/* The `natural` block of the JSON report (a complete member, no trailing
 * comma). */
void natural_bench_write_json(FILE *f);

int natural_bench_frame_count(void);

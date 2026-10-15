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

/* One natural frame is one LVGL display refresh period. The ROM configures
 * LV_DISP_DEF_REFR_PERIOD 50 ms (lv_conf.h); the harness' own lv_conf.h does not
 * set it, so the harness runs at LVGL's default 30 ms and a natural frame is
 * 30 ms of virtual time. tests/check_harness_wakeups.sh measures the ROM's loop
 * with the ROM's own period (wakeup_bench.c) instead of this one. The clock
 * advances in slices of NATURAL_BENCH_SLICE_MS and the data drive is checked once
 * per slice (the same 10 ms the plain capture uses). */
#define NATURAL_BENCH_SLICE_MS 10

/* Frames a natural run does when neither the fixture nor the CLI say. */
#define NATURAL_BENCH_FRAMES_DEFAULT 40

/* Invalidation rectangles one frame can carry; the frames of the main screen
 * have at most one per component that changed (spectrum, waterfall, clock). A
 * frame with more sets `rects_complete` to false: the sums stay complete, only
 * the reported rectangles are capped. */
#define NATURAL_BENCH_MAX_RECTS 8

/* The radio's DSP loop feeds the spectrum and one waterfall row on its own
 * cadence: `spectrum_fps_ms = 1000 / 15` and `waterfall_fps_ms = 1000 / 25`
 * (src/dsp.cpp), with the `now - last > fps_ms` condition. The harness does not
 * compile dsp.cpp (there is no FFT and no SDR), so the cadence is mirrored here;
 * tests/check_harness_natural.sh recomputes the expected frame/row counts from
 * these same expressions on the produced JSON, so a drift between this mirror
 * and the radio's loop is caught by the gate. The wakeup profile
 * (wakeup_bench.c) feeds the same cadence, so the two modes measure the same
 * radio. */
#define NATURAL_SPECTRUM_FPS_MS  (1000 / 15)
#define NATURAL_WATERFALL_FPS_MS (1000 / 25)

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
 * control of tests/check_harness_natural.sh. The wakeup profile switches it for
 * its two passes. */
void natural_bench_set_drive(bool on);
bool natural_bench_drive(void);

/* One slice of the virtual clock: the samples of the radio's DSP loop arrive
 * here. Feeding them queues the component's own refresh (spectrum_data() and
 * waterfall_data() call scheduler_put_noargs), which the scheduler runs right
 * after LVGL's timers: the invalidation is the firmware's, never the bench's.
 * Exported for the wakeup profile (wakeup_bench.c), which runs the ROM's loop
 * over the same cadence. */
void natural_bench_drive_slice(void *arg);

/* Re-arms the drive with `seed` at the current virtual instant: the same
 * counters the natural run starts from (spectrum/waterfall times and row
 * counts), so a run of the profile is a function of the seed and the window.
 * Used by the wakeup profile's two passes (wakeup_bench.c). */
void natural_bench_drive_reset(uint32_t seed);

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

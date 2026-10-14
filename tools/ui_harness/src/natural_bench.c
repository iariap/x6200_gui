/*
 *  Natural-invalidation bench for the host UI harness (see natural_bench.h).
 *
 *  Why it exists: the forced bench fabricates its own work
 *  (`lv_obj_invalidate(lv_scr_act())` + `lv_refr_now()` per frame), so the
 *  firmware's own invalidation is invisible to it — measured in UI-07
 *  increment 1, halving the invalidated area of the spectrum moved neither
 *  `flushes` nor `redrawn_area`, and the plain capture feeds the data once
 *  (screens.c) so the spectrum/waterfall never invalidate again. This mode
 *  closes both gaps without touching `src/` of the ROM:
 *
 *    - it runs LVGL's natural loop over the virtual clock: no forced
 *      invalidation, no `lv_refr_now()`. The refresh is the one LVGL's own
 *      refresh timer decides, from the invalidations the real components
 *      queued through the scheduler;
 *    - the spectrum/waterfall magnitudes are driven frame by frame by the
 *      deterministic generator (signal_gen.c), on the radio's own cadence
 *      (src/dsp.cpp); the clock invalidates every 500 ms (src/clock.c);
 *    - per frame it reports the invalidation LVGL collected for the refresh
 *      cycle (its own buffer, the same trace UI-07 increment 1 used as a
 *      temporary diagnostic) and what the display driver was asked to flush.
 *
 *  Determinism: every value in the section is a function of the scenario's seed
 *  and the frame count. No wall clock is read, so two runs write the same bytes.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "natural_bench.h"

#include <string.h>

#include "signal_gen.h"
#include "virtual_time.h"

#include "cfg/subjects.h"
#include "scheduler.h"

/* The radio's DSP loop feeds the spectrum and one waterfall row on its own
 * cadence: `spectrum_fps_ms = 1000 / 15` and `waterfall_fps_ms = 1000 / 25`
 * (src/dsp.cpp), with the `now - last > fps_ms` condition. The harness does not
 * compile dsp.cpp (there is no FFT and no SDR), so the cadence is mirrored here;
 * tests/check_harness_natural.sh recomputes the expected frame/row counts from
 * these same expressions on the produced JSON, so a drift between this mirror
 * and the radio's loop is caught by the gate. */
#define NATURAL_SPECTRUM_FPS_MS  (1000 / 15)
#define NATURAL_WATERFALL_FPS_MS (1000 / 25)

/* Upper bound of a run's frames (the record array is fixed). */
#define NATURAL_BENCH_MAX_FRAMES 512

static bool enabled;
static bool drive = true;

static bool            frame_open;
static int             total_frames;
static int             current_frame;
static natural_frame_t frames[NATURAL_BENCH_MAX_FRAMES];

/* Data drive state: the same two timers dsp.cpp keeps. */
static uint32_t seed;
static uint64_t spectrum_time;
static uint64_t waterfall_time;
static uint32_t spectrum_frames;
static uint32_t waterfall_rows;

static uint32_t area_size(const lv_area_t *area)
{
    return (uint32_t) (area->x2 - area->x1 + 1) * (uint32_t) (area->y2 - area->y1 + 1);
}

void natural_bench_enable(bool on)
{
    enabled = on;
}

bool natural_bench_enabled(void)
{
    return enabled;
}

void natural_bench_set_drive(bool on)
{
    drive = on;
}

bool natural_bench_drive(void)
{
    return drive;
}

/* One slice of the virtual clock: the samples of the radio's DSP loop arrive
 * here. Feeding them queues the component's own refresh (spectrum_data() and
 * waterfall_data() call scheduler_put_noargs), which the scheduler runs right
 * after LVGL's timers: the invalidation is the firmware's, never the bench's. */
static void natural_slice(void *arg)
{
    (void) arg;

    if (!drive) {
        return;
    }

    uint64_t now = virtual_time_now_ms();

    if (now - spectrum_time > NATURAL_SPECTRUM_FPS_MS) {
        signal_gen_spectrum_frame(seed, (int) spectrum_frames);
        spectrum_frames++;
        spectrum_time = now;
    }
    if (now - waterfall_time > NATURAL_WATERFALL_FPS_MS) {
        signal_gen_waterfall_row(seed, (int) waterfall_rows);
        waterfall_rows++;
        waterfall_time = now;
    }
}

void natural_bench_run(const scenario_t *scenario, int count)
{
    if (count < 1) {
        count = 1;
    }
    if (count > NATURAL_BENCH_MAX_FRAMES) {
        count = NATURAL_BENCH_MAX_FRAMES;
    }

    memset(frames, 0, sizeof(frames));
    for (int i = 0; i < count; i++) {
        frames[i].rects_complete = true;
    }

    seed = scenario->seed;
    total_frames = count;
    spectrum_frames = 0;
    waterfall_rows = 0;
    spectrum_time = virtual_time_now_ms();
    waterfall_time = spectrum_time;
    enabled = true;

    for (current_frame = 0; current_frame < total_frames; current_frame++) {
        frame_open = true;
        virtual_time_advance_step(LV_DISP_DEF_REFR_PERIOD, NATURAL_BENCH_SLICE_MS, natural_slice,
                                  NULL);
        frames[current_frame].virtual_end_ms = virtual_time_now_ms();
        frame_open = false;
    }
}

void natural_bench_flush(const lv_area_t *area, const lv_disp_drv_t *drv)
{
    if (!enabled || !frame_open || current_frame >= total_frames) {
        return;
    }

    natural_frame_t *frame = &frames[current_frame];

    frame->flushes++;
    frame->flushed_px += area_size(area);

    /* Every flush of a refresh cycle sees the same invalidation list, so the
     * cycle is recorded only when LVGL's draw buffer says the last part of the
     * last invalidated area is being drawn. */
    if (!drv->draw_buf || !drv->draw_buf->last_area || !drv->draw_buf->last_part) {
        return;
    }

    lv_disp_t *disp = lv_disp_get_default();

    if (!disp) {
        return;
    }

    /* LVGL clears the invalidation list after the areas are drawn
     * (lv_refr.c: `inv_p = 0` once `refr_invalid_areas()` returns), so this is
     * the trace of what the firmware invalidated by itself in this cycle. A
     * joined rectangle is a duplicate LVGL merged into another one: counting it
     * twice would count the same pixels twice (it is reported separately). */
    frame->refresh_cycles++;

    for (uint16_t i = 0; i < disp->inv_p; i++) {
        if (disp->inv_area_joined[i]) {
            frame->joined_px += area_size(&disp->inv_areas[i]);
            continue;
        }

        frame->invalidated_px += area_size(&disp->inv_areas[i]);
        if (frame->invalidated_rects < NATURAL_BENCH_MAX_RECTS) {
            frame->rects[frame->invalidated_rects] = disp->inv_areas[i];
        } else {
            frame->rects_complete = false;
        }
        frame->invalidated_rects++;
    }
}

int natural_bench_frame_count(void)
{
    return total_frames;
}

static void write_totals(FILE *f)
{
    uint64_t invalidated = 0;
    uint64_t rects = 0;
    uint64_t joined = 0;
    uint64_t flushed = 0;
    uint64_t flushes = 0;
    uint64_t cycles = 0;
    uint64_t frames_with_flush = 0;

    for (int i = 0; i < total_frames; i++) {
        const natural_frame_t *frame = &frames[i];

        invalidated += frame->invalidated_px;
        rects += frame->invalidated_rects;
        joined += frame->joined_px;
        flushed += frame->flushed_px;
        flushes += frame->flushes;
        cycles += frame->refresh_cycles;
        if (frame->flushes > 0) {
            frames_with_flush++;
        }
    }

    fprintf(f, "    \"totals\": {\n");
    fprintf(f, "      \"invalidated_px\": %llu,\n", (unsigned long long) invalidated);
    fprintf(f, "      \"invalidated_rects\": %llu,\n", (unsigned long long) rects);
    fprintf(f, "      \"joined_px\": %llu,\n", (unsigned long long) joined);
    fprintf(f, "      \"flushed_px\": %llu,\n", (unsigned long long) flushed);
    fprintf(f, "      \"flushes\": %llu,\n", (unsigned long long) flushes);
    fprintf(f, "      \"refresh_cycles\": %llu,\n", (unsigned long long) cycles);
    fprintf(f, "      \"frames_with_flush\": %llu\n", (unsigned long long) frames_with_flush);
    fprintf(f, "    }\n");
}

void natural_bench_write_json(FILE *f)
{
    fprintf(f, "  \"natural\": {\n");
    fprintf(f, "    \"enabled\": true,\n");
    fprintf(f, "    \"clock\": \"virtual-time\",\n");
    fprintf(f, "    \"environment\": \"x86 host: the channel/ARM energy cost is not measured\",\n");
    fprintf(f, "    \"frame_ms\": %d,\n", LV_DISP_DEF_REFR_PERIOD);
    fprintf(f, "    \"slice_ms\": %d,\n", NATURAL_BENCH_SLICE_MS);
    fprintf(f, "    \"frames\": %d,\n", total_frames);
    fprintf(f, "    \"data_drive\": %s,\n", drive ? "true" : "false");
    fprintf(f, "    \"seed\": %u,\n", seed);
    fprintf(f, "    \"spectrum_fps_ms\": %d,\n", NATURAL_SPECTRUM_FPS_MS);
    fprintf(f, "    \"waterfall_fps_ms\": %d,\n", NATURAL_WATERFALL_FPS_MS);
    fprintf(f, "    \"spectrum_frames\": %u,\n", spectrum_frames);
    fprintf(f, "    \"waterfall_rows\": %u,\n", waterfall_rows);
    fprintf(f, "    \"per_frame\": [\n");

    for (int i = 0; i < total_frames; i++) {
        const natural_frame_t *frame = &frames[i];

        fprintf(f,
                "      { \"frame\": %d, \"virtual_end_ms\": %llu, \"refresh_cycles\": %u, "
                "\"invalidated_px\": %u, \"invalidated_rects\": %u, \"joined_px\": %u, "
                "\"flushed_px\": %u, \"flushes\": %u, \"rects_complete\": %s, \"rects\": [",
                i, (unsigned long long) frame->virtual_end_ms, frame->refresh_cycles,
                frame->invalidated_px, frame->invalidated_rects, frame->joined_px,
                frame->flushed_px, frame->flushes, frame->rects_complete ? "true" : "false");
        for (uint32_t r = 0;
             r < frame->invalidated_rects && r < NATURAL_BENCH_MAX_RECTS; r++) {
            fprintf(f, "%s[%d,%d,%d,%d]", r ? ", " : "",
                    frame->rects[r].x1, frame->rects[r].y1,
                    frame->rects[r].x2 - frame->rects[r].x1 + 1,
                    frame->rects[r].y2 - frame->rects[r].y1 + 1);
        }
        fprintf(f, "] }%s\n", i + 1 < total_frames ? "," : "");
    }

    fprintf(f, "    ],\n");
    write_totals(f);
    fprintf(f, "  }\n");
}

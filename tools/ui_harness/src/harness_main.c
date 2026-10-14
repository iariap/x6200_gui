/*
 *  ui_harness: headless LVGL renderer for the X6200 GUI.
 *
 *  Usage: ui_harness --scenario <file> --png <out.png> --json <out.json>
 *                    [--bench [--warmup N] [--repeats N]]
 *
 *  Builds a scenario from real firmware components on an 800x480 RAM
 *  framebuffer, advances the harness' virtual clock (no sleeps), writes the
 *  PNG and a JSON with the git revision, the per-component coverage and the
 *  render metrics.
 *
 *  With `--bench` it also measures the real render cost of each frame with the
 *  monotonic clock (see bench_metrics.h) and writes the per-frame timings plus
 *  the process memory into the JSON. Benchmarking never feeds a wall-clock
 *  value back into the GUI, so the PNG of a scenario is the same with and
 *  without `--bench`.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "lvgl/lvgl.h"

#include "bench_metrics.h"
#include "clock.h"
#include "display.h"
#include "events.h"
#include "params/params.h"
#include "png_export.h"
#include "render_metrics.h"
#include "scenario.h"
#include "styles.h"
#include "theme.h"
#include "fonts/font_select.h"
#include "coverage.h"
#include "virtual_time.h"

#include "mock_harness.h"
#include "mock_radio.h"

#ifndef HARNESS_REVISION
#define HARNESS_REVISION "unknown"
#endif

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --scenario <file> --png <out.png> [--json <out.json>]\n"
            "          [--bench [--warmup N] [--repeats N]]\n",
            argv0);
}

static long file_size(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0) {
        return -1;
    }
    return (long) st.st_size;
}

static void write_benchmark(FILE *f, const scenario_t *scenario)
{
    bench_memory_t memory = bench_read_memory();

    fprintf(f, "  \"benchmark\": {\n");
    fprintf(f, "    \"enabled\": true,\n");
    fprintf(f, "    \"clock\": \"CLOCK_MONOTONIC\",\n");
    fprintf(f, "    \"environment\": \"x86 host: relative regression proxy, not the Cortex-A7\",\n");
    fprintf(f, "    \"warmup_frames\": %d,\n", scenario->bench_warmup);
    fprintf(f, "    \"repeats\": %d,\n", scenario->bench_repeats);
    fprintf(f, "    \"redraw_passes\": %d,\n", scenario->bench_redraw_passes);
    fprintf(f, "    \"frames\": [\n");
    for (size_t i = 0; i < bench_frame_count(); i++) {
        const bench_frame_t *frame = bench_frame_at(i);

        fprintf(f,
                "      { \"render_ms\": %.4f, \"flush_ms\": %.4f, \"draw_ms\": %.4f, "
                "\"flushes\": %u, \"redrawn_area_px\": %u, \"unique_redrawn_px\": %u }%s\n",
                frame->render_ms, frame->flush_ms, frame->draw_ms,
                frame->flushes, frame->redrawn_area_px, frame->unique_redrawn_px,
                i + 1 < bench_frame_count() ? "," : "");
    }
    fprintf(f, "    ],\n");
    fprintf(f, "    \"memory\": {\n");
    fprintf(f, "      \"rss_bytes\": %llu,\n", (unsigned long long) memory.rss_bytes);
    fprintf(f, "      \"pss_bytes\": %llu,\n", (unsigned long long) memory.pss_bytes);
    fprintf(f, "      \"peak_rss_bytes\": %llu,\n", (unsigned long long) memory.peak_rss_bytes);
    fprintf(f, "      \"peak_vm_bytes\": %llu\n", (unsigned long long) memory.peak_vm_bytes);
    fprintf(f, "    }\n");
    fprintf(f, "  }\n");
}

static void write_json(const char *path, const char *png_path, const scenario_t *scenario)
{
    FILE *f = fopen(path, "w");

    if (!f) {
        fprintf(stderr, "ui_harness: cannot write %s\n", path);
        return;
    }

    int count = 0;
    const coverage_entry_t *cov = coverage_main_screen(&count);

    fprintf(f, "{\n");
    fprintf(f, "  \"scenario\": \"%s\",\n", scenario->name);
    fprintf(f, "  \"screen\": \"%s\",\n", scenario->screen);
    fprintf(f, "  \"seed\": %u,\n", scenario->seed);
    fprintf(f, "  \"revision\": \"%s\",\n", HARNESS_REVISION);
    fprintf(f, "  \"framebuffer\": { \"width\": %u, \"height\": %u },\n",
            display_width(), display_height());
    fprintf(f, "  \"virtual_time_ms\": %llu,\n",
            (unsigned long long) virtual_time_now_ms());
    fprintf(f, "  \"coverage\": [\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "    { \"component\": \"%s\", \"level\": \"%s\", \"note\": \"%s\" }%s\n",
                cov[i].component, coverage_level_name(cov[i].level), cov[i].note,
                i + 1 < count ? "," : "");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"radio_calls\": {\n");
    fprintf(f, "    \"get_state\": %u,\n", mock_radio_calls.get_state);
    fprintf(f, "    \"set_freq\": %u,\n", mock_radio_calls.set_freq);
    fprintf(f, "    \"check_freq\": %u,\n", mock_radio_calls.check_freq);
    fprintf(f, "    \"toggle_vfo\": %u,\n", mock_radio_calls.toggle_vfo);
    fprintf(f, "    \"set_ptt\": %u,\n", mock_radio_calls.set_ptt);
    fprintf(f, "    \"set_pwr\": %u,\n", mock_radio_calls.set_pwr);
    fprintf(f, "    \"change_vol\": %u,\n", mock_radio_calls.change_vol);
    fprintf(f, "    \"change_mic\": %u,\n", mock_radio_calls.change_mic);
    fprintf(f, "    \"change_mute\": %u,\n", mock_radio_calls.change_mute);
    fprintf(f, "    \"start_atu\": %u,\n", mock_radio_calls.start_atu);
    fprintf(f, "    \"poweroff\": %u\n", mock_radio_calls.poweroff);
    fprintf(f, "  },\n");
    fprintf(f, "  \"metrics\": {\n");
    fprintf(f, "    \"flushes\": %u,\n", render_metrics_flush_count());
    fprintf(f, "    \"flushed_pixels\": %llu,\n",
            (unsigned long long) render_metrics_flushed_pixels());
    fprintf(f, "    \"framebuffer_fnv1a\": \"0x%016llx\",\n",
            (unsigned long long) png_framebuffer_hash());
    fprintf(f, "    \"png_path\": \"%s\",\n", png_path);
    fprintf(f, "    \"png_bytes\": %ld\n", file_size(png_path));
    fprintf(f, "  }%s\n", bench_enabled() ? "," : "");
    if (bench_enabled()) {
        write_benchmark(f, scenario);
    }
    fprintf(f, "}\n");
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *scenario_path = NULL;
    const char *png_path = NULL;
    const char *json_path = NULL;
    bool        bench = false;
    int         warmup_override = -1;
    int         repeats_override = -1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--scenario") && i + 1 < argc) {
            scenario_path = argv[++i];
        } else if (!strcmp(argv[i], "--png") && i + 1 < argc) {
            png_path = argv[++i];
        } else if (!strcmp(argv[i], "--json") && i + 1 < argc) {
            json_path = argv[++i];
        } else if (!strcmp(argv[i], "--bench")) {
            bench = true;
        } else if (!strcmp(argv[i], "--warmup") && i + 1 < argc) {
            warmup_override = (int) strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--repeats") && i + 1 < argc) {
            repeats_override = (int) strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "ui_harness: unknown argument '%s'\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (!scenario_path || !png_path) {
        usage(argv[0]);
        return 2;
    }

    mock_params_init();
    mock_cfg_init();
    mock_radio_init();

    scenario_t scenario;
    memset(&scenario, 0, sizeof(scenario));
    snprintf(scenario.name, sizeof(scenario.name), "unnamed");
    snprintf(scenario.screen, sizeof(scenario.screen), "main");
    scenario.seed = 1;
    scenario.bench_warmup = SCENARIO_BENCH_WARMUP_DEFAULT;
    scenario.bench_repeats = SCENARIO_BENCH_REPEATS_DEFAULT;
    scenario.bench_redraw_passes = SCENARIO_BENCH_REDRAW_PASSES_DEFAULT;

    if (!scenario_load(scenario_path, &scenario)) {
        return 1;
    }

    if (warmup_override > 0) {
        scenario.bench_warmup = warmup_override;
    }
    if (repeats_override > 0) {
        scenario.bench_repeats = repeats_override;
    }

    virtual_time_reset(1000);
    render_metrics_reset();

    lv_init();
    display_init();

    /* Allocate the GUI's custom LVGL event ids before any screen registers a
     * callback for them (the firmware's main does the same). */
    event_init();

    theme_init(params.theme_name.x);
    fonts_apply(params.font.x);
    styles_init(THEME_SIMPLE);

    if (!scenario_build(&scenario)) {
        return 1;
    }

    /* Let the components' timers run on the virtual clock, then force a full
     * repaint so the framebuffer holds the settled screen. */
    virtual_time_advance(1200, 10);

    if (bench) {
        bench_enable(true);

        /* Warm-up frames: the allocator, caches and the drawn layers settle
         * here; they are not measured. */
        for (int i = 0; i < scenario.bench_warmup; i++) {
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now(NULL);
        }
        for (int r = 0; r < scenario.bench_repeats; r++) {
            bench_frame_begin();
            uint64_t start = bench_now_ns();

            for (int p = 0; p < scenario.bench_redraw_passes; p++) {
                lv_obj_invalidate(lv_scr_act());
                lv_refr_now(NULL);
            }
            bench_frame_end((double) (bench_now_ns() - start) / 1000000.0);
        }
    } else {
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(NULL);
    }

    if (!png_export(png_path)) {
        return 1;
    }

    if (json_path) {
        write_json(json_path, png_path, &scenario);
    }

    printf("ui_harness: %s -> %s (%ld bytes), flushes=%u, flushed_pixels=%llu\n",
           scenario.name, png_path, file_size(png_path),
           render_metrics_flush_count(),
           (unsigned long long) render_metrics_flushed_pixels());

    if (bench) {
        printf("ui_harness: benchmark frames=%zu (warmup=%d, repeats=%d, redraw_passes=%d)\n",
               bench_frame_count(), scenario.bench_warmup, scenario.bench_repeats,
               scenario.bench_redraw_passes);
    }
    return 0;
}

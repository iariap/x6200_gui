/*
 *  ui_harness: headless LVGL renderer for the X6200 GUI.
 *
 *  Usage: ui_harness --scenario <file> --png <out.png> --json <out.json>
 *                    [--bench [--warmup N] [--repeats N]] [--inspect]
 *                    [--bench-natural [N]] [--natural-frames N]
 *                    [--natural-static] [--bench-wakeups [MS]] [--display-off]
 *
 *  Builds a scenario from real firmware components on an 800x480 RAM
 *  framebuffer, advances the harness' virtual clock (no sleeps), writes the
 *  PNG and a JSON with the git revision, the per-component coverage and the
 *  render metrics.
 *
 *  With `--inspect` the JSON also carries the structural inspection of the live
 *  LVGL object tree (see ui_inspector.h): bounding boxes with the real clipping,
 *  widget class, label text with its font metrics and the glyphs that fall back
 *  or are missing. It never changes a rendered pixel.
 *
 *  With `--bench` it also measures the real render cost of each frame with the
 *  monotonic clock (see bench_metrics.h) and writes the per-frame timings plus
 *  the process memory into the JSON. Benchmarking never feeds a wall-clock
 *  value back into the GUI, so the PNG of a scenario is the same with and
 *  without `--bench`.
 *
 *  With `--bench-natural [N]` (or a fixture's `natural_frames N`) the harness
 *  runs LVGL's natural loop for N frames of virtual time instead: no forced
 *  invalidation, the spectrum/waterfall magnitudes driven frame by frame by the
 *  deterministic generator on the radio's own cadence, and the JSON gains a
 *  `natural` block with the invalidation of every frame (see natural_bench.h).
 *  It is an extra mode: the `benchmark` block, the PNG path and the plain
 *  capture are untouched. `--natural-static` runs the same loop without the
 *  data drive (the delimitation control of tests/check_harness_natural.sh).
 *
 *  With `--bench-wakeups [MS]` the harness measures the ROM's *main loop*
 *  (src/main.c) over MS milliseconds of virtual time —default 1000— on the
 *  firmware's own timer set, and reports the tick thread's cadence next to it in
 *  a `wakeups` block (see wakeup_bench.h). Con `LV_TICK_CUSTOM` en el lv_conf.h de
 *  la ROM el hilo del tick no existe y el perfil declara `tick_custom: true` con 0
 *  wakeups de tick (el CMake deriva el modo de ese mismo archivo). It answers the
 *  wakeup question of issue #13 without touching the ROM. `--display-off` runs
 *  the ROM's own display-off route (the POWER key -> backlight_switch()) before
 *  the capture/bench, so the bench can measure what the blanked screen does and
 *  does not render (tests/check_harness_natural.sh).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lvgl/lvgl.h"

#include "bench_metrics.h"
#include "clock.h"
#include "display.h"
#include "events.h"
#include "natural_bench.h"
#include "params/params.h"
#include "png_export.h"
#include "render_metrics.h"
#include "scenario.h"
#include "styles.h"
#include "theme.h"
#include "fonts/font_select.h"
#include "coverage.h"
#include "ui_inspector.h"
#include "virtual_time.h"
#include "wakeup_bench.h"

#include "mock_harness.h"
#include "mock_radio.h"

#ifndef HARNESS_REVISION
#define HARNESS_REVISION "unknown"
#endif

#ifndef HARNESS_THEMES_DIR_SHIPPED
#define HARNESS_THEMES_DIR_SHIPPED "/usr/share/x6200/themes"
#endif

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --scenario <file> --png <out.png> [--json <out.json>]\n"
            "          [--bench [--warmup N] [--repeats N]] [--inspect]\n"
            "          [--bench-natural [N]] [--natural-frames N] [--natural-static]\n"
            "          [--bench-wakeups [MS]] [--display-off]\n",
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

/* The theme of this capture, as the firmware would see it.
 *
 * `theme_list()` (src/theme.c) offers names up to 31 characters — the built-ins
 * and the JSON files — and the choice travels to the next boot inside
 * `params.theme_name`, a `params_str_t` whose `x[]` is 32 bytes
 * (src/params/types.h), enough for every name the menu can offer. (Before UI-013
 * it was 16 bytes and "Outdoor-Negative" did not fit.) `requested` is the name
 * this capture rendered (what the fixture asked for); `params_name` is what the
 * firmware's own store holds, read from the `params_str_t` the mock shares with
 * the firmware, so its capacity and its truncation are the firmware's. A name
 * that does not survive the round trip makes `theme_init(params.theme_name.x)`
 * (src/main.c) fall back to IU1WXY. */
static void write_theme(FILE *f, const scenario_t *scenario)
{
    fprintf(f, "  \"theme\": {\n");
    fprintf(f, "    \"requested\": \"%s\",\n",
            scenario->theme[0] ? scenario->theme : params.theme_name.x);
    fprintf(f, "    \"params_name\": \"%s\",\n", params.theme_name.x);
    fprintf(f, "    \"params_max_chars\": %zu,\n", sizeof(params.theme_name.x) - 1);
    fprintf(f, "    \"active\": \"%s\"\n", theme_shape.name);
    fprintf(f, "  },\n");
}

static void write_json(const char *path, const char *png_path, const scenario_t *scenario,
                       bool inspect)
{
    FILE *f = fopen(path, "w");

    if (!f) {
        fprintf(stderr, "ui_harness: cannot write %s\n", path);
        return;
    }

    int count = 0;
    const coverage_entry_t *cov = coverage_for_screen(scenario->screen, &count);

    fprintf(f, "{\n");
    fprintf(f, "  \"scenario\": \"%s\",\n", scenario->name);
    fprintf(f, "  \"screen\": \"%s\",\n", scenario->screen);
    fprintf(f, "  \"seed\": %u,\n", scenario->seed);
    fprintf(f, "  \"revision\": \"%s\",\n", HARNESS_REVISION);
    fprintf(f, "  \"framebuffer\": { \"width\": %u, \"height\": %u },\n",
            display_width(), display_height());
    write_theme(f, scenario);
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
    fprintf(f, "  }%s\n",
            (bench_enabled() || natural_bench_enabled() || wakeup_bench_enabled() || inspect)
                ? "," : "");
    if (bench_enabled()) {
        write_benchmark(f, scenario);
        if (natural_bench_enabled() || wakeup_bench_enabled() || inspect) {
            fprintf(f, ",\n");
        }
    }
    if (natural_bench_enabled()) {
        natural_bench_write_json(f);
        if (wakeup_bench_enabled() || inspect) {
            fprintf(f, ",\n");
        }
    }
    if (wakeup_bench_enabled()) {
        wakeup_bench_write_json(f);
        if (inspect) {
            fprintf(f, ",\n");
        }
    }
    if (inspect) {
        fprintf(f, "  \"inspection\": ");
        ui_inspector_write_json(f, lv_scr_act());
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
    bool        inspect = false;
    bool        natural = false;
    bool        natural_static = false;
    bool        wakeups = false;
    bool        display_off = false;
    int         warmup_override = -1;
    int         repeats_override = -1;
    int         natural_frames_override = -1;
    uint32_t    wakeup_window_ms = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--scenario") && i + 1 < argc) {
            scenario_path = argv[++i];
        } else if (!strcmp(argv[i], "--png") && i + 1 < argc) {
            png_path = argv[++i];
        } else if (!strcmp(argv[i], "--json") && i + 1 < argc) {
            json_path = argv[++i];
        } else if (!strcmp(argv[i], "--bench")) {
            bench = true;
        } else if (!strcmp(argv[i], "--inspect")) {
            inspect = true;
        } else if (!strcmp(argv[i], "--bench-natural")) {
            natural = true;
            /* An explicit count may follow (`--bench-natural 60`); without it
             * the fixture's `natural_frames` (or the default) decides. */
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                natural_frames_override = (int) strtol(argv[++i], NULL, 10);
            }
        } else if (!strcmp(argv[i], "--natural-frames") && i + 1 < argc) {
            natural = true;
            natural_frames_override = (int) strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--natural-static")) {
            natural = true;
            natural_static = true;
        } else if (!strcmp(argv[i], "--bench-wakeups")) {
            wakeups = true;
            /* An explicit window in ms may follow (`--bench-wakeups 5000`);
             * without it the built-in default decides. A window that is not a
             * positive count is a typo, never a default. */
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                long v = strtol(argv[++i], NULL, 10);

                if (v <= 0) {
                    fprintf(stderr, "ui_harness: --bench-wakeups needs a positive window\n");
                    return 2;
                }
                wakeup_window_ms = (uint32_t) v;
            }
        } else if (!strcmp(argv[i], "--display-off")) {
            display_off = true;
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

    /* The two loop-measuring modes are separate runs of the same loop: asking
     * for both at once would measure one on top of the other. */
    if (wakeups && natural) {
        fprintf(stderr, "ui_harness: --bench-wakeups and --bench-natural are separate modes\n");
        return 2;
    }

    /* A non-positive count is a typo, never a default: without a number the
     * fixture's `natural_frames` (or the built-in default) decides. */
    if (natural_frames_override == 0 || natural_frames_override < -1) {
        fprintf(stderr, "ui_harness: --bench-natural needs a positive frame count\n");
        return 2;
    }

    mock_params_init();
    mock_cfg_init();
    mock_radio_init();

    /* `src/dialog_settings.c` fills its date/time fields with localtime(), which
     * depends on TZ. The harness reads nothing from the environment, so the zone
     * is pinned to UTC; together with the fixed epoch of `__wrap_time()`
     * (mocks/mock_ui_deps.c) the Ajustes date/time fields are identical on every
     * host and every run. */
    setenv("TZ", "UTC", 1);
    tzset();

    scenario_t scenario;
    memset(&scenario, 0, sizeof(scenario));
    snprintf(scenario.name, sizeof(scenario.name), "unnamed");
    snprintf(scenario.screen, sizeof(scenario.screen), "main");
    scenario.seed = 1;
    scenario.bench_warmup = SCENARIO_BENCH_WARMUP_DEFAULT;
    scenario.bench_repeats = SCENARIO_BENCH_REPEATS_DEFAULT;
    scenario.bench_redraw_passes = SCENARIO_BENCH_REDRAW_PASSES_DEFAULT;
    scenario.natural_frames = NATURAL_BENCH_FRAMES_DEFAULT;

    if (!scenario_load(scenario_path, &scenario)) {
        return 1;
    }

    if (warmup_override > 0) {
        scenario.bench_warmup = warmup_override;
    }
    if (repeats_override > 0) {
        scenario.bench_repeats = repeats_override;
    }
    if (natural_frames_override > 0) {
        scenario.natural_frames = natural_frames_override;
    }

    virtual_time_reset(1000);
    render_metrics_reset();

    lv_init();
    display_init();

    /* Allocate the GUI's custom LVGL event ids before any screen registers a
     * callback for them (the firmware's main does the same). */
    event_init();

    /* The ROM ships its themes with the image; read them from this repository
     * (the same files) so a fixture can name one (RS, Holo, Material 3) without
     * depending on an installed /usr/share/x6200. An explicit THEMES_DIR_SHIPPED
     * still wins (overwrite=0). */
    setenv("THEMES_DIR_SHIPPED", HARNESS_THEMES_DIR_SHIPPED, 0);

    theme_init(scenario.theme[0] ? scenario.theme : params.theme_name.x);

    /* Same font choice as the firmware's main(): a theme's own font first, then
     * the parameter, and VISION scales the whole family up. Before this the
     * harness applied params.font only, so a theme that names a font (Classic,
     * RS) or VISION drew with the wrong one. */
    if (theme_shape.font >= 0) {
        fonts_apply((uint8_t) theme_shape.font);
    } else if (params.font.x > 0) {
        fonts_apply((uint8_t) (params.font.x - 1));
    }
    if (theme_shape.vision) {
        fonts_scale(VISION_FONT_STEPS);
    }

    styles_init(THEME_SIMPLE);

    if (!scenario_build(&scenario)) {
        return 1;
    }

    /* Let the components' timers run on the virtual clock, then force a full
     * repaint so the framebuffer holds the settled screen. */
    virtual_time_advance(1200, 10);

    /* Fixture work that needs the timers to have run: moving the focus once the
     * real main screen has registered its widget in the keyboard group. */
    scenario_settle(&scenario);

    /* Pantalla apagada (`--display-off`): la tecla POWER de la ROM, despues de
     * que la pantalla ya se dibujo (el panel conserva el ultimo frame y lo que
     * se apaga es el backlight). */
    if (display_off) {
        scenario_display_off();
    }

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

    if (natural) {
        /* The natural bench runs *after* the settled full repaint above (which
         * is not measured), so every frame of the run only carries the work the
         * real components trigger on their own timers. It never forces an
         * invalidation and never calls lv_refr_now(). */
        natural_bench_set_drive(!natural_static);
        natural_bench_run(&scenario, scenario.natural_frames);
    }

    if (wakeups) {
        /* El perfil de wakeups (wakeup_bench.c) corre tambien despues del
         * repaint del settle: mide el lazo de la ROM sobre los timers reales de
         * los componentes, no un frame fabricado. */
        wakeup_bench_run(&scenario, wakeup_window_ms);
    }

    if (!png_export(png_path)) {
        return 1;
    }

    if (json_path) {
        write_json(json_path, png_path, &scenario, inspect);
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
    if (natural_bench_enabled()) {
        printf("ui_harness: natural frames=%d (frame_ms=%d, drive=%s)\n",
               natural_bench_frame_count(), LV_DISP_DEF_REFR_PERIOD,
               natural_bench_drive() ? "on" : "off");
    }
    if (wakeup_bench_enabled()) {
        printf("ui_harness: wakeups window=%u ms (refresh_period_ms=%u, tick_thread_ms=%u)\n",
               wakeup_bench_window_ms(), wakeup_bench_refresh_period_ms(),
               wakeup_bench_tick_thread_ms());
    }
    return 0;
}

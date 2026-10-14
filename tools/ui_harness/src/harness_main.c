/*
 *  ui_harness: headless LVGL renderer for the X6200 GUI.
 *
 *  Usage: ui_harness --scenario <file> --png <out.png> --json <out.json>
 *
 *  Builds a scenario from real firmware components on an 800x480 RAM
 *  framebuffer, advances the harness' virtual clock (no sleeps), writes the
 *  PNG and a JSON with the git revision, the per-component coverage and the
 *  render metrics.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "lvgl/lvgl.h"

#include "clock.h"
#include "display.h"
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
            "usage: %s --scenario <file> --png <out.png> [--json <out.json>]\n",
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

static void write_json(const char *path, const char *png_path, const scenario_t *scenario)
{
    FILE *f = fopen(path, "w");

    if (!f) {
        fprintf(stderr, "ui_harness: cannot write %s\n", path);
        return;
    }

    int count = 0;
    const coverage_entry_t *cov = coverage_home_usb_20m(&count);

    fprintf(f, "{\n");
    fprintf(f, "  \"scenario\": \"%s\",\n", scenario->name);
    fprintf(f, "  \"screen\": \"%s\",\n", scenario->screen);
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
    fprintf(f, "  \"metrics\": {\n");
    fprintf(f, "    \"flushes\": %u,\n", render_metrics_flush_count());
    fprintf(f, "    \"flushed_pixels\": %llu,\n",
            (unsigned long long) render_metrics_flushed_pixels());
    fprintf(f, "    \"framebuffer_fnv1a\": \"0x%016llx\",\n",
            (unsigned long long) png_framebuffer_hash());
    fprintf(f, "    \"png_path\": \"%s\",\n", png_path);
    fprintf(f, "    \"png_bytes\": %ld\n", file_size(png_path));
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *scenario_path = NULL;
    const char *png_path = NULL;
    const char *json_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--scenario") && i + 1 < argc) {
            scenario_path = argv[++i];
        } else if (!strcmp(argv[i], "--png") && i + 1 < argc) {
            png_path = argv[++i];
        } else if (!strcmp(argv[i], "--json") && i + 1 < argc) {
            json_path = argv[++i];
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
    snprintf(scenario.screen, sizeof(scenario.screen), "home-usb-20m");

    if (!scenario_load(scenario_path, &scenario)) {
        return 1;
    }

    virtual_time_reset(1000);
    render_metrics_reset();

    lv_init();
    display_init();

    theme_init(params.theme_name.x);
    fonts_apply(params.font.x);
    styles_init(THEME_SIMPLE);

    if (!scenario_build(&scenario)) {
        return 1;
    }

    /* Let the components' timers run on the virtual clock, then force a full
     * repaint so the framebuffer holds the settled screen. */
    virtual_time_advance(1200, 10);
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);

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
    return 0;
}

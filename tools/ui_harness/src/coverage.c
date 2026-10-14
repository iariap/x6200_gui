/*
 *  Component coverage for the host UI harness (see coverage.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "coverage.h"

#define COMPONENT(c, l, n) { .component = (c), .level = (l), .note = (n) }

static const coverage_entry_t home_usb_20m[] = {
    COMPONENT("theme.c", COVERAGE_REAL,
              "src/theme.c compiled; theme_init(IU1WXY) sets the builtin colours"),
    COMPONENT("styles.c", COVERAGE_REAL,
              "src/styles.c compiled; the screen is styled by the real styles"),
    COMPONENT("fonts/", COVERAGE_REAL,
              "every src/fonts/*.c compiled and used by the labels"),
    COMPONENT("clock.c", COVERAGE_REAL,
              "src/clock.c compiled and drawn; shown in battery mode"),
    COMPONENT("info.c", COVERAGE_REAL,
              "src/info.c compiled and drawn (VFO, mode, AGC, PRE/ATT, ATU, WiFi, BT)"),
    COMPONENT("meter.c", COVERAGE_REAL,
              "src/meter.c compiled and drawn; needle S-meter"),
    COMPONENT("band_info.c", COVERAGE_PARTIAL,
              "src/band_info.c compiled, not placed in this scenario's layout"),
    COMPONENT("params", COVERAGE_SYNTHETIC,
              "src/params/params.c not compiled; harness owns params_t with fixed values"),
    COMPONENT("cfg", COVERAGE_SYNTHETIC,
              "src/cfg/* not compiled; harness owns cfg_t/cfg_cur with the subjects the components read"),
    COMPONENT("radio", COVERAGE_SYNTHETIC,
              "mocks/mock_radio.c: explicit radio state, no device"),
    COMPONENT("spectrum.c", COVERAGE_UNSUPPORTED,
              "not rendered in milestone 1 (needs the DSP/FFT path)"),
    COMPONENT("waterfall.c", COVERAGE_UNSUPPORTED,
              "not rendered in milestone 1"),
    COMPONENT("main_screen.c", COVERAGE_UNSUPPORTED,
              "the full main screen is not linked in milestone 1; only its top-bar components are"),
    COMPONENT("buttons.cpp", COVERAGE_UNSUPPORTED,
              "the bottom button bar is not rendered in milestone 1"),
};

const coverage_entry_t *coverage_home_usb_20m(int *count)
{
    *count = (int) (sizeof(home_usb_20m) / sizeof(home_usb_20m[0]));
    return home_usb_20m;
}

const char *coverage_level_name(coverage_level_t level)
{
    switch (level) {
        case COVERAGE_REAL:        return "REAL";
        case COVERAGE_PARTIAL:     return "PARTIAL";
        case COVERAGE_SYNTHETIC:   return "SYNTHETIC";
        default:                   return "UNSUPPORTED";
    }
}

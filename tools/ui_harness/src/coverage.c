/*
 *  Component coverage for the host UI harness (see coverage.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <string.h>

#include "coverage.h"

#define COMPONENT(c, l, n) { .component = (c), .level = (l), .note = (n) }

/* Components every main-screen scenario shares, whatever the theme: the main
 * screen and the visual components it places are the real firmware sources,
 * drawn against the explicit mocks. */
#define MAIN_COMPONENTS \
    COMPONENT("theme.c", COVERAGE_REAL, \
              "src/theme.c compiled; theme_init() sets the scenario's theme (builtin or the ROM's JSON)"), \
    COMPONENT("styles.c", COVERAGE_REAL, \
              "src/styles.c compiled; every object is styled by the real styles"), \
    COMPONENT("fonts/", COVERAGE_REAL, \
              "every src/fonts/*.c compiled and used by the labels"), \
    COMPONENT("main_screen.c", COVERAGE_REAL, \
              "src/main_screen.c compiled; main_screen() builds the whole screen"), \
    COMPONENT("spectrum.c", COVERAGE_PARTIAL, \
              "src/spectrum.c compiled and drawn; the magnitudes come from the seed-driven generator (src/signal_gen.c), not from an FFT/radio"), \
    COMPONENT("waterfall.c", COVERAGE_PARTIAL, \
              "src/waterfall.c compiled and drawn; the per-row magnitudes come from the seed-driven generator, not from an FFT/radio"), \
    COMPONENT("band_info.c", COVERAGE_PARTIAL, \
              "src/band_info.c compiled and drawn; the band rows come from the harness catalogue (mock_cfg), not from the SQLite DB"), \
    COMPONENT("band_plan.c", COVERAGE_REAL, \
              "src/band_plan.c + band_plan_data.c compiled and drawn (IARU strip)"), \
    COMPONENT("meter.c", COVERAGE_REAL, \
              "src/meter.c compiled and drawn; needle S-meter"), \
    COMPONENT("info.c", COVERAGE_REAL, \
              "src/info.c compiled and drawn (VFO, mode, AGC, PRE/ATT, ATU, WiFi, BT)"), \
    COMPONENT("clock.c", COVERAGE_REAL, \
              "src/clock.c compiled and drawn (battery / power view)"), \
    COMPONENT("tx_info.c", COVERAGE_REAL, \
              "src/tx_info.c compiled and drawn (TX info, hidden while RX)"), \
    COMPONENT("buttons.cpp", COVERAGE_REAL, \
              "src/buttons.cpp compiled; the bottom button bar is drawn"), \
    COMPONENT("pannel.c", COVERAGE_REAL, \
              "src/pannel.c compiled and drawn (the overlay panel, empty here)"), \
    COMPONENT("msg.c", COVERAGE_REAL, \
              "src/msg.c + msg_tiny.c compiled and drawn (empty: the harness sends no messages)"), \
    COMPONENT("adaptive_tuning.c", COVERAGE_REAL, \
              "src/adaptive_tuning.c compiled and initialised; the tuning-multiplier label is created (hidden)"), \
    COMPONENT("cw_tune_ui.c", COVERAGE_PARTIAL, \
              "src/cw_tune_ui.c compiled; hidden because CW tune is off in these fixtures"), \
    COMPONENT("cfg/subjects.cpp", COVERAGE_REAL, \
              "src/cfg/subjects.cpp compiled; the real Subject/observer core backs cfg_cur/cfg"), \
    COMPONENT("cfg (catalogue)", COVERAGE_SYNTHETIC, \
              "no database: mock_cfg owns cfg_t/cfg_cur and its values (no src/cfg/db.cpp)"), \
    COMPONENT("params", COVERAGE_SYNTHETIC, \
              "src/params/params.c not compiled; harness owns params_t with fixed values"), \
    COMPONENT("radio", COVERAGE_SYNTHETIC, \
              "mocks/mock_radio.c: explicit radio state, no device")

static const coverage_entry_t main_screen[] = {
    MAIN_COMPONENTS,
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
    COMPONENT("freq_strip.c", COVERAGE_UNSUPPORTED,
              "the app frequency strip is not rendered; main_screen draws its frequency row inline"),
    COMPONENT("dialogs (dialog_*.c)", COVERAGE_UNSUPPORTED,
              "app dialogs are not opened; dialog_construct() opens none of them in these fixtures"),
};

/* The `dialog-about` scenario: the same main screen with the real About window
 * over it. The shared dialog machinery and that one window are real; the rest
 * of the app dialogs keep their inert mock descriptors. */
static const coverage_entry_t dialog_about[] = {
    MAIN_COMPONENTS,
    COMPONENT("dialog.c", COVERAGE_REAL,
              "src/dialog.c compiled; dialog_construct()/dialog_init() build the real dialog box over the screen"),
    COMPONENT("dialog_about.c", COVERAGE_REAL,
              "src/dialog_about.c compiled; the real About window (title, version, callsign, author labels)"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "the other app dialogs are inert mocks; only the About path is compiled and opened"),
    COMPONENT("freq_strip.c", COVERAGE_UNSUPPORTED,
              "the app frequency strip is not rendered; main_screen draws its frequency row inline"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

/* The `settings-home` / `settings-wfbat` scenarios: the same main screen with the
 * real Ajustes window over it. The shared dialog machinery, the window itself
 * and every widget it places are the real firmware sources; the few services the
 * window calls (mixer volume, the DSP presets, the wfview server, the RTC) are
 * the harness' explicit mocks, and the three transverter rows read harness
 * subjects because there is no database. */
static const coverage_entry_t settings_dialog[] = {
    MAIN_COMPONENTS,
    COMPONENT("dialog.c", COVERAGE_REAL,
              "src/dialog.c compiled; dialog_construct()/dialog_init()/dialog_item() build the real dialog box over the screen"),
    COMPONENT("dialog_settings.c", COVERAGE_REAL,
              "src/dialog_settings.c compiled; the whole Ajustes grid is built and drawn (date/time, clock, backlight, info, audio, spectrum, waterfall, operations, appearance, actions)"),
    COMPONENT("audio.c", COVERAGE_SYNTHETIC,
              "the mixer surface only: audio_set_play_vol() returns the requested level (src/audio.c does the same when the ALSA mixer is absent); no device"),
    COMPONENT("audio_enhancement.c", COVERAGE_SYNTHETIC,
              "the preset load is a harness no-op; no speex/liquid DSP runs (mocks/mock_ui_deps.c)"),
    COMPONENT("wfview_server.c", COVERAGE_SYNTHETIC,
              "no /mnt/wfview.conf and no socket: the server reports off (mocks/mock_ui_deps.c)"),
    COMPONENT("cfg/transverter.c", COVERAGE_SYNTHETIC,
              "the three transverter rows read harness subjects with the firmware defaults; no DB (mocks/mock_cfg.c)"),
    COMPONENT("RTC / system clock (time())", COVERAGE_SYNTHETIC,
              "the date/time fields read a fixed epoch, not the host clock (CMakeLists `--wrap=time`); the radio's RTC is another replaced service"),
    COMPONENT("backlight.c", COVERAGE_SYNTHETIC,
              "the backlight setters are no-ops on the host (mocks/mock_ui_deps.c): the Ajustes backlight row draws, it does not change a level"),
    COMPONENT("bt_state.c", COVERAGE_SYNTHETIC,
              "Bluetooth off (mocks/mock_ui_deps.c): the two Bluetooth switches draw unchecked"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "the other app dialogs are inert mocks; only the About and Ajustes paths are compiled and opened"),
    COMPONENT("dialog_field.c", COVERAGE_UNSUPPORTED,
              "not a dependency of the Ajustes window: src/dialog_settings.c does not include it, and the harness links the window without it (the earlier 'Ajustes arrastra dialog_field' note was wrong)"),
    COMPONENT("freq_strip.c", COVERAGE_UNSUPPORTED,
              "the app frequency strip is not rendered; main_screen draws its frequency row inline"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

const coverage_entry_t *coverage_main_screen(int *count)
{
    *count = (int) (sizeof(main_screen) / sizeof(main_screen[0]));
    return main_screen;
}

const coverage_entry_t *coverage_for_screen(const char *screen, int *count)
{
    if (screen && !strcmp(screen, "dialog-about")) {
        *count = (int) (sizeof(dialog_about) / sizeof(dialog_about[0]));
        return dialog_about;
    }
    if (screen && (!strcmp(screen, "settings-home") || !strcmp(screen, "settings-wfbat"))) {
        *count = (int) (sizeof(settings_dialog) / sizeof(settings_dialog[0]));
        return settings_dialog;
    }
    return coverage_main_screen(count);
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

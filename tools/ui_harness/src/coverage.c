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
    COMPONENT("cfg/settings_manager.cpp", COVERAGE_REAL, \
              "src/cfg/settings_manager.cpp compiled; the remote control's mirror of the configuration binds its parameters to the real subjects at boot and is checked there (mocks/mock_settings_manager.cpp): reading it returns the stored value, not the neutral one (issue #30)"), \
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

/* The `settings-home` / `settings-wfbat` / `settings-perfil` / `settings-audio`
 * scenarios: the same main screen with the real Ajustes window over it. The shared
 * dialog machinery, the window itself and every widget it places are the real
 * firmware sources; the few services the window calls (mixer volume, the DSP
 * presets, the wfview server, the RTC) are the harness' explicit mocks, and the
 * three transverter rows read harness subjects because there is no database. */
static const coverage_entry_t settings_dialog[] = {
    MAIN_COMPONENTS,
    COMPONENT("dialog.c", COVERAGE_REAL,
              "src/dialog.c compiled; dialog_construct()/dialog_init()/dialog_item() build the real dialog box over the screen"),
    COMPONENT("dialog_settings.c", COVERAGE_REAL,
              "src/dialog_settings.c compiled; the whole Ajustes grid is built and drawn (date/time, clock, backlight, info, audio, spectrum, waterfall, operations, appearance, actions)"),
    COMPONENT("audio.c", COVERAGE_SYNTHETIC,
              "the mixer surface only: audio_set_play_vol()/audio_set_rec_vol() return the requested level (src/audio.c does the same when the ALSA mixer is absent) and record which one ran, which is what the settings-audio fixture asserts; no device"),
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
              "the other app dialogs are inert mocks; only the Ajustes path is compiled and opened"),
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

/* The `app-bands` scenario: the main screen with the app menu the APP key opens
 * and the "Bands" button pressed. The button bar and the main screen are real;
 * `dialog_bands` is the inert mock, so the fixture measures the route the button
 * takes (src/main_screen.c -> src/buttons.cpp -> main_screen_start_app) and not
 * the window's pixels. */
static const coverage_entry_t app_bands[] = {
    MAIN_COMPONENTS,
    COMPONENT("main_screen.c (ACTION_APP_BANDS)", COVERAGE_PARTIAL,
              "src/main_screen.c compiled; the app-menu button's path to main_screen_start_app(ACTION_APP_BANDS) runs and is followed through dialog_is_run() in scenario_settle, not through the Bands window"),
    COMPONENT("dialog.c", COVERAGE_REAL,
              "src/dialog.c compiled; when the route reaches the Bands path dialog_construct()/dialog_init() run the real dialog lifecycle"),
    COMPONENT("dialog_bands.c", COVERAGE_UNSUPPORTED,
              "src/dialog_bands.c is NOT compiled: dialog_bands is an inert mock descriptor (mocks/mock_main_screen.c). The Bands window is not rendered and nothing of it is measured"),
    COMPONENT("dialog_field.c / field/bands.c / cluster/spots.c", COVERAGE_UNSUPPORTED,
              "the Bands window's dependencies are not compiled either; with dialog_bands.c unsupported there is nothing of that subtree to render"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "the other app dialogs are inert mocks; only the Ajustes path is compiled and opened"),
    COMPONENT("freq_strip.c", COVERAGE_UNSUPPORTED,
              "the app frequency strip is not rendered; main_screen draws its frequency row inline"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

/* The `settings-bands` scenario: the real Ajustes window over the main screen and
 * then the real Bands action run from there. The shared dialog machinery and the
 * Ajustes window are real; the Bands window itself is NOT: `dialog_bands` is one
 * of the inert mock descriptors (mocks/mock_main_screen.c), so the fixture
 * measures the route the firmware takes (src/screens.c, scenario_settle) and not
 * the window's pixels. */
static const coverage_entry_t settings_bands[] = {
    MAIN_COMPONENTS,
    COMPONENT("dialog.c", COVERAGE_REAL,
              "src/dialog.c compiled; dialog_construct()/dialog_init()/dialog_destruct() run the real dialog lifecycle the route uses"),
    COMPONENT("dialog_settings.c", COVERAGE_REAL,
              "src/dialog_settings.c compiled; the Ajustes window is built and drawn before the Bands action closes it"),
    COMPONENT("main_screen.c (ACTION_APP_BANDS)", COVERAGE_PARTIAL,
              "src/main_screen.c compiled and its dispatcher runs; the action is followed through dialog_is_run()/dialog_settings->run in scenario_settle, not through the Bands window"),
    COMPONENT("dialog_bands.c", COVERAGE_UNSUPPORTED,
              "src/dialog_bands.c is NOT compiled: dialog_bands is an inert mock descriptor (mocks/mock_main_screen.c). The Bands window is not rendered and nothing of it is measured"),
    COMPONENT("dialog_field.c / field/bands.c / cluster/spots.c", COVERAGE_UNSUPPORTED,
              "the Bands window's dependencies are not compiled either; with dialog_bands.c unsupported there is nothing of that subtree to render"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "the other app dialogs are inert mocks; only the Ajustes path is compiled and opened"),
    COMPONENT("freq_strip.c", COVERAGE_UNSUPPORTED,
              "the app frequency strip is not rendered; main_screen draws its frequency row inline"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

/* The `apps-menu` scenarios (UI-002): the main screen with the app menu the APP
 * key opens, on one of its six pages. The button bar and the main screen are
 * real; the fixtures measure the menu itself (the page's cells draw their app
 * names) and not any app window. */
static const coverage_entry_t apps_menu[] = {
    MAIN_COMPONENTS,
    COMPONENT("main_screen.c (APP key -> buttons_group_app)", COVERAGE_PARTIAL,
              "src/main_screen.c compiled; the APP key's path loads buttons_group_app and the page button cycles its six pages, but the keypress itself is driven by buttons_load_page_group()/buttons_press() from the fixture, not by the panel keypad"),
    COMPONENT("buttons.cpp (app pages)", COVERAGE_REAL,
              "src/buttons.cpp compiled; the six APP pages and their app buttons are the real firmware table, drawn in the bottom bar"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "the fixtures open no app dialog: the app buttons are counted in the bar, not pressed"),
    COMPONENT("freq_strip.c", COVERAGE_UNSUPPORTED,
              "the app frequency strip is not rendered; main_screen draws its frequency row inline"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

/* The `bt-dialog` scenario (UI-005): the real Bluetooth window (src/dialog_bt.c)
 * opened over the main screen. The dialog, its table and its status line are the
 * real firmware sources; `btaudio-ctl` is replaced by a mock that seeds the file
 * the script would leave (mocks/mock_btaudio.c) and `system()` is wrapped to a
 * no-op, so no script runs on the host. */
static const coverage_entry_t bt_dialog[] = {
    MAIN_COMPONENTS,
    COMPONENT("dialog.c", COVERAGE_REAL,
              "src/dialog.c compiled; dialog_construct()/dialog_init() run the real dialog lifecycle over the screen"),
    COMPONENT("dialog_bt.c", COVERAGE_REAL,
              "src/dialog_bt.c compiled; the Bluetooth window (header, device table, status line and the Scan/RX/Speaker/Auto/Close row) is built and drawn"),
    COMPONENT("bt_devices.c", COVERAGE_REAL,
              "src/bt_devices.c compiled; the file the script leaves is parsed by the real parser"),
    COMPONENT("btaudio.c", COVERAGE_SYNTHETIC,
              "the script surface is a mock: btaudio_log()/btaudio_speaker_mute() are no-ops and system() is wrapped (mocks/mock_btaudio.c), so no btaudio-ctl runs and the list file is seeded explicitly"),
    COMPONENT("buttons.cpp", COVERAGE_REAL,
              "src/buttons.cpp compiled; the dialog's own bottom bar (Scan/RX/Speaker/Auto/Close) is drawn"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "only the Bluetooth dialog is compiled and opened; the rest are inert mocks"),
    COMPONENT("dialog_field.c", COVERAGE_UNSUPPORTED,
              "not a dependency of the Bluetooth window"),
    COMPONENT("freq_strip.c", COVERAGE_UNSUPPORTED,
              "the app frequency strip is not rendered; main_screen draws its frequency row inline"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

const coverage_entry_t *coverage_for_screen(const char *screen, int *count)
{
    if (screen && (!strcmp(screen, "settings-home") || !strcmp(screen, "settings-wfbat")
                   || !strcmp(screen, "settings-perfil") || !strcmp(screen, "settings-audio"))) {
        *count = (int) (sizeof(settings_dialog) / sizeof(settings_dialog[0]));
        return settings_dialog;
    }
    if (screen && !strcmp(screen, "settings-bands")) {
        *count = (int) (sizeof(settings_bands) / sizeof(settings_bands[0]));
        return settings_bands;
    }
    if (screen && !strcmp(screen, "app-bands")) {
        *count = (int) (sizeof(app_bands) / sizeof(app_bands[0]));
        return app_bands;
    }
    if (screen && !strcmp(screen, "apps-menu")) {
        *count = (int) (sizeof(apps_menu) / sizeof(apps_menu[0]));
        return apps_menu;
    }
    if (screen && !strcmp(screen, "bt-dialog")) {
        *count = (int) (sizeof(bt_dialog) / sizeof(bt_dialog[0]));
        return bt_dialog;
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

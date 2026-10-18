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
              "src/theme.c compiled; theme_init() sets the scenario's theme (a ROM JSON, docs/THEMES.md)"), \
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
              "src/cw_tune_ui.c compiled; the widget is built with the main screen and its visibility follows the rig's mode (CW/CWR only, issue #122: the mode decides, not cw_tune), so it is hidden outside CW/CWR"), \
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
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

/* The `app-close-home` scenario (menu-hygiene 5/6): the real main screen with
 * RTTY opened and closed through the firmware's own route
 * (main_screen_start_app(ACTION_APP_RTTY) -> main_screen_close_app()) and, in
 * between, the real Ajustes window. `scenario_settle` codifies what the route
 * must leave behind (RTTY off and the bar back on the home page). RTTY itself is
 * not compiled on the host: its state is a documented mock
 * (mocks/mock_main_screen.c) that now tracks the set/reset the route performs. */
static const coverage_entry_t app_close[] = {
    MAIN_COMPONENTS,
    COMPONENT("main_screen.c (ACTION_APP_RTTY close)", COVERAGE_PARTIAL,
              "src/main_screen.c compiled; the non-dialog app (RTTY) is opened and closed by the same functions the remote control calls, but the RTTY codec is a state-only mock"),
    COMPONENT("dialog.c", COVERAGE_REAL,
              "src/dialog.c compiled; dialog_construct()/dialog_destruct() run the real dialog lifecycle the fixture opens over RTTY"),
    COMPONENT("dialog_settings.c", COVERAGE_REAL,
              "src/dialog_settings.c compiled; the real Ajustes window is built over RTTY and then closed"),
    COMPONENT("rtty.c", COVERAGE_SYNTHETIC,
              "src/rtty.c not compiled; the state (RTTY_OFF/RTTY_RX) is a documented mock (mocks/mock_main_screen.c) whose only observable is what the route sets it to"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "only the Ajustes path is compiled and opened; the rest are inert mocks"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

/* The `autotune` / `autotune-progress` / `autotune-found` scenarios (issue
 * #100): the real main screen with the "Auto Tune" button of the APP 6 page
 * pressed. The assistant core (src/autotune.c) and its driver
 * (src/autotune_ui.c) are the real firmware sources, run by the fixture on the
 * virtual clock; the S-meter they consume is the harness' explicit mock
 * (mock_radio.smeter_db), and `autotune-found` pushes a synthetic peak because
 * the bank has no DSP to update it. The final message is not drawn: the
 * harness' event_send() is a documented no-op (mocks/mock_ui_deps.c). */
static const coverage_entry_t autotune_screen[] = {
    MAIN_COMPONENTS,
    COMPONENT("autotune.c", COVERAGE_REAL,
              "src/autotune.c compiled; the real scoring, the zigzag plan and the deterministic restore run over the probe frequencies the fixture walks"),
    COMPONENT("autotune_ui.c", COVERAGE_REAL,
              "src/autotune_ui.c compiled; the real driver retunes cfg_cur.fg_freq probe by probe and reads the S-meter, on the fixture's virtual clock"),
    COMPONENT("main_screen.c (ACTION_ATUNE)", COVERAGE_REAL,
              "src/main_screen.c compiled; the APP 6 Auto Tune button reaches main_screen_action(ACTION_ATUNE) -> autotune_ui_toggle(), the same dispatcher the radio uses"),
    COMPONENT("meter.c (S-meter)", COVERAGE_PARTIAL,
              "src/meter.c compiled; meter_get_raw_db() is the signal the assistant scores, but the level is the fixture's (mock_radio.smeter_db, constant; autotune-found pushes a synthetic peak), not a real RF reading"),
    COMPONENT("msg.c (final message)", COVERAGE_UNSUPPORTED,
              "the final message goes through event_send(), a documented no-op on the host (mocks/mock_ui_deps.c): it is not drawn in the bank, so the dial is the fixture's evidence"),
    COMPONENT("dialog.c (cancel on open)", COVERAGE_UNSUPPORTED,
              "src/dialog.c compiled and autotune_ui_cancel() is wired into dialog_construct(); these fixtures open no dialog, so that path is not exercised here"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "the assistant opens no dialog; it runs over the main screen"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

/* The `swrscan` scenarios (issue #125): the real SWR Scan window over the real
 * main screen, driven by the fake ROE acquisition backend (src/swr_backend.c).
 * The window, its session engine and the resample of the curve are the real
 * firmware sources; the radio side is the harness' counter-only mock and the
 * datasets are the owner's fixtures (tests/fixtures/swrscan/). */
static const coverage_entry_t swrscan_screen[] = {
    MAIN_COMPONENTS,
    COMPONENT("dialog.c", COVERAGE_REAL,
              "src/dialog.c compiled; dialog_construct()/dialog_init() run the real dialog lifecycle over the screen"),
    COMPONENT("dialog_swrscan.c", COVERAGE_REAL,
              "src/dialog_swrscan.c compiled; the whole window is built and drawn (chart, Run/Scale/Span/Passes buttons, status and readout) and its own gate decides whether the session starts"),
    COMPONENT("swr_session.c", COVERAGE_REAL,
              "src/swr_session.c compiled; the grid, the per-frequency aggregation and the measured minimum are the real engine, fed one flow packet per point"),
    COMPONENT("roe_curve.c", COVERAGE_PARTIAL,
              "src/roe_curve.c compiled; the drawn curve is the quadratic fit over the aggregated samples (decorative, broken at the gaps), while the minimum and the readout come from the measured data (swr_session.c), never from the fit"),
    COMPONENT("acquisition (fake backend)", COVERAGE_PARTIAL,
              "the radio's side of the contract is src/swr_backend.c replaying tests/fixtures/swrscan/swr_mock_samples.csv on the virtual clock: the frequencies the dialog asks for are checked against the dataset point by point, but they are the fixture's numbers, not a reading from an instrument"),
    COMPONENT("radio", COVERAGE_SYNTHETIC,
              "mocks/mock_radio.c: radio_start_swrscan()/radio_stop_swrscan()/radio_set_freq() only count. The binary links no RF library, so a fixture has no path to a transmission (the JSON reports the counters)"),
    COMPONENT("scheduler", COVERAGE_SYNTHETIC,
              "the teardown the window queues when the session ends (scheduler_put_noargs) runs synchronously on the virtual clock (mocks/mock_main_screen.c) instead of on the radio's worker thread"),
    COMPONENT("settle window / timeouts", COVERAGE_SYNTHETIC,
              "the stall watchdog runs on the virtual clock (the backend calls dialog_swrscan_poll() after every packet); no real wait is measured, so the settle window of a real VFO is not validated here (requires-radio)"),
    COMPONENT("multipass TX gate", COVERAGE_PARTIAL,
              "SWRSCAN_MULTIPASS_TX_ENABLED is compiled ON in this host binary (tools/ui_harness/CMakeLists.txt) so the 3- and 5-pass captures exist at all; the ROM binary keeps it off (src/CMakeLists.txt passes no such define) and test_swrscan_multipass_gate pins that. Enabling multipass TX for real needs the radio (requires-radio)"),
    COMPONENT("RF / TX", COVERAGE_UNSUPPORTED,
              "there is no radio on the host: the transmitted power, the real VFO settling and the real VSWR bridge are not measured. What the bank proves is that no transmission is reachable from a fixture (0 real TX), not that the sweep is safe on hardware (requires-radio)"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "the other app dialogs are inert mocks; only the SWR Scan window is compiled and opened"),
};

/* The `cw-tune-default` / `cw-tune-tuner` / `cw-tune-ssb` scenarios (issue #122):
 * the real main screen with the "(CW 1:2)" page of the key group loaded, and in
 * `cw-tune-tuner` the "Tuner" button pressed. `cw-tune-ssb` is the same widget with
 * the rig in USB, the other half of the visibility rule. The widget
 * (src/cw_tune_ui.c) and the button (src/buttons.cpp, controls_toggle_cw_tuner) are
 * the real firmware sources; the tone that draws the active block is synthetic (the
 * bank has no DSP), and the fixtures assert the mode, the parameter, the button
 * label and the widget's visibility. */
static const coverage_entry_t cw_tune[] = {
    MAIN_COMPONENTS,
    COMPONENT("cw_tune_ui.c", COVERAGE_REAL,
              "src/cw_tune_ui.c compiled; cw_tune_init() builds the widget and update_visibility() drives its hidden flag from the mode, which the fixtures assert (shown in CW with cw_tune=0, hidden in USB, plus the cw_tune_ui_visible() the tone feed consults); the active block's tone is synthetic (cw_tune_set_freq(8 Hz) from the fixture), since the bank has no DSP"),
    COMPONENT("buttons.cpp (CW page)", COVERAGE_REAL,
              "src/buttons.cpp compiled; the key group's `page_cw_decoder_1` (the page labelled (CW 1:2), Decoder / Tuner / Dec SN R) is the real table, and its Tuner button runs controls_toggle_cw_tuner() -> cfg.cw_tune on the user's path"),
    COMPONENT("main_screen.c (KEY key -> buttons_group_key)", COVERAGE_PARTIAL,
              "src/main_screen.c compiled; the KEY key's path loads buttons_group_key, but the keypress itself is driven by buttons_load_page_group()/buttons_press() from the fixture, not by the panel keypad"),
    COMPONENT("cw.cpp (tone feed)", COVERAGE_UNSUPPORTED,
              "src/cw.cpp is not compiled: the DSP that pushes the tuning tone (update_peak_freq -> cw_tune_set_freq) does not run on the host, so the fixtures call cw_tune_set_freq() directly and tests/check_cw_tune_feed.py codifies its gate over the source"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "the fixtures open no app dialog: the CW page is measured in the bottom bar"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

/* Los `cw-autotune-off` / `cw-autotune-on` (issue #139): la pagina (CW 2:2) de
 * la barra, la real, con el boton "Auto" junto a Peak/Noise beta. El boton, su
 * toggle y su rotulo son fuentes reales del firmware; el auto-ajuste en si vive
 * en src/cw.cpp + src/cw_autotune.c, que el arnes NO compila (necesitan
 * liquid-dsp y el lazo de audio): el fixture mide la UI y el modulo se prueba
 * entero en la suite nativa. */
static const coverage_entry_t cw_autotune[] = {
    MAIN_COMPONENTS,
    COMPONENT("buttons.cpp (CW page 2)", COVERAGE_REAL,
              "src/buttons.cpp compiled; `page_cw_decoder_2` (the page labelled (CW 2:2): Peak beta / Noise beta / Auto) is the real table, and the Auto button runs controls_toggle_cw_decoder_auto() -> cfg.cw_decoder_auto on the user's path (the fixture presses index 3)"),
    COMPONENT("cfg.cw_decoder_auto", COVERAGE_SYNTHETIC,
              "mock_cfg creates the cfg item with its firmware default (0, off); there is no params.db on the host, so the persistence across a reboot is measured in the QEMU bank"),
    COMPONENT("cw_autotune.c / cw.cpp (AUTO logic)", COVERAGE_UNSUPPORTED,
              "neither file is compiled here: src/cw.cpp needs liquid-dsp and the audio thread, and src/cw_autotune.c is exercised by tests/native/test_cw_autotune.c (synthetic windows, clamps, deadband, tone sign). The fixture does not claim convergence: that is requires-radio"),
    COMPONENT("cw_tune_ui.c", COVERAGE_PARTIAL,
              "src/cw_tune_ui.c compiled; the tuning widget is part of the same CW screen and its visibility follows the rig's mode"),
    COMPONENT("dialogs (other apps)", COVERAGE_UNSUPPORTED,
              "the fixtures open no app dialog: the (CW 2:2) page is measured in the bottom bar"),
    COMPONENT("spot_marks.c", COVERAGE_UNSUPPORTED,
              "DX cluster spot marks are not rendered (no cluster data on the host)"),
    COMPONENT("cluster_column.c", COVERAGE_UNSUPPORTED,
              "DX cluster column is not rendered (no cluster data on the host)"),
};

const coverage_entry_t *coverage_for_screen(const char *screen, int *count)
{
    if (screen && !strcmp(screen, "swrscan")) {
        *count = (int) (sizeof(swrscan_screen) / sizeof(swrscan_screen[0]));
        return swrscan_screen;
    }
    if (screen && (!strcmp(screen, "cw-autotune-off") || !strcmp(screen, "cw-autotune-on"))) {
        *count = (int) (sizeof(cw_autotune) / sizeof(cw_autotune[0]));
        return cw_autotune;
    }
    if (screen
        && (!strcmp(screen, "cw-tune-default") || !strcmp(screen, "cw-tune-tuner")
            || !strcmp(screen, "cw-tune-ssb"))) {
        *count = (int) (sizeof(cw_tune) / sizeof(cw_tune[0]));
        return cw_tune;
    }
    if (screen && (!strcmp(screen, "autotune") || !strcmp(screen, "autotune-progress")
                   || !strcmp(screen, "autotune-found"))) {
        *count = (int) (sizeof(autotune_screen) / sizeof(autotune_screen[0]));
        return autotune_screen;
    }
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
    if (screen && !strcmp(screen, "app-close-home")) {
        *count = (int) (sizeof(app_close) / sizeof(app_close[0]));
        return app_close;
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

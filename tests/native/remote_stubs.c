/*
 * Stubs for the native src/remote_server.cpp test (tests/native/test_remote.cpp).
 *
 * The server under test is staged next to minimal stubs (tests/native/stubs_remote/),
 * the same pattern as the clock/backlight/digital-modes/SWR-Scan/restore/events/hkey/
 * spectrum tests. Here the collaborators of the socket are inert: the radio is always
 * in RX, no dialog is open and there is no band table. The test only drives the two
 * paths issue #157 is about -- the mode table (parse_cmd/state_line) and the power
 * read in the state line -- plus the rest of the command contract.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "buttons.h"
#include "dialog.h"
#include "gps.h"
#include "main_screen.h"
#include "meter.h"
#include "radio.h"
#include "rtty.h"
#include "scheduler.h"
#include "util.h"
#include "waterfall.h"
#include "cluster/client.h"

#include "cfg/cfg.h"
#include "params/params.h"

/* Configuration subjects the test wires (see remote_wire() in test_remote.cpp). */
cfg_t     cfg;
cfg_cur_t cfg_cur;

params_t params;

/* Bottom-bar page groups (parse_cmd's "page" command only needs them to exist). */
buttons_page_t *buttons_group_gen[] = {NULL};
buttons_page_t *buttons_group_app[] = {NULL};
buttons_page_t *buttons_group_key[] = {NULL};
buttons_page_t *buttons_group_dfn[] = {NULL};
buttons_page_t *buttons_group_dfl[] = {NULL};

/* The dialogs current_app() probes. None is open. */
dialog_t *dialog_ft8      = NULL;
dialog_t *dialog_js8      = NULL;
dialog_t *dialog_wspr     = NULL;
dialog_t *dialog_sstv     = NULL;
dialog_t *dialog_wefax    = NULL;
dialog_t *dialog_navtex   = NULL;
dialog_t *dialog_field    = NULL;
dialog_t *dialog_cw_kbd   = NULL;
dialog_t *dialog_gps      = NULL;
dialog_t *dialog_bands    = NULL;
dialog_t *dialog_settings = NULL;
dialog_t *dialog_bt       = NULL;
dialog_t *dialog_cluster  = NULL;
dialog_t *dialog_cw_trainer = NULL;
dialog_t *dialog_eq       = NULL;
dialog_t *dialog_status   = NULL;
dialog_t *dialog_qth      = NULL;
dialog_t *dialog_callsign = NULL;
dialog_t *dialog_recorder = NULL;
dialog_t *dialog_wifi     = NULL;
dialog_t *dialog_swrscan  = NULL;
dialog_t *dialog_logs     = NULL;
dialog_t *dialog_propagation = NULL;

void buttons_press(uint8_t n, bool hold) { (void)n; (void)hold; }
void buttons_load_page_group(buttons_group_t group) { (void)group; }

bool dialog_type_is_run(dialog_t *dialog) { (void)dialog; return false; }
void dialog_restore_radio_on_close(void) { }
void dialog_restore_mode_on_close(int32_t mode, int32_t forced) { (void)mode; (void)forced; }

bool cw_tune_recent(int32_t *hz) { (void)hz; return false; }

void gps_phone_set(double lat, double lon) { (void)lat; (void)lon; }
void gps_phone_clear(void) { }

void main_screen_set_freq(uint64_t f) { (void)f; }
bool main_screen_freq_locked(void) { return false; }
bool main_screen_mode_locked(void) { return false; }
bool main_screen_band_locked(void) { return false; }
void main_screen_agc_next(void) { }
void main_screen_start_app(press_action_t page_app) { (void)page_app; }
void main_screen_close_app(void) { }

int16_t meter_get_raw_db(void) { return -120; }

radio_state_t radio_get_state(void) { return RADIO_RX; }
uint16_t      radio_change_vol(int16_t df) { (void)df; return 0; }

rtty_state_t rtty_get_state(void) { return RTTY_OFF; }

bool scheduler_put(scheduler_fn_t fn, void *arg, size_t arg_size) {
    (void)fn;
    (void)arg;
    (void)arg_size;
    return true;
}

void waterfall_get_levels(bool tx, float *min, float *max) {
    (void)tx;
    if (min) *min = 0.0f;
    if (max) *max = 1.0f;
}

void cluster_client_feed_line(const char *line) { (void)line; }

uint64_t get_time(void) { return 0; }
int32_t  align_int(int32_t x, uint16_t step) { (void)step; return x; }

/* The C bridge src/cfg/settings_manager.cpp uses to validate a frequency
 * (src/cfg/cxx_bridge.c in the firmware). Any in-band frequency is fine here. */
bool x6200_bridge_radio_check_freq(int32_t freq) {
    return freq >= 100000 && freq <= 500000000;
}

/* The app commands / "ax" state the server's tables point at: never called with no app open. */
void dialog_ft8_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_ft8_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
void dialog_cw_kbd_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_cw_kbd_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
void dialog_js8_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_js8_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
void dialog_wspr_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_wspr_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
void dialog_sstv_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_sstv_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
void dialog_wefax_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_wefax_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
void dialog_field_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_field_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
bool dialog_field_qso_log(const char *text) { (void)text; return true; }
void dialog_navtex_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_navtex_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
void dialog_swrscan_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_swrscan_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
bool dialog_swrscan_remote_run(bool start) { (void)start; return false; }
void dialog_propagation_remote(const char *cmd, int32_t val, const char *text) { (void)cmd; (void)val; (void)text; }
int  dialog_propagation_remote_state(char *buf, size_t size) { (void)buf; (void)size; return 0; }
bool dialog_propagation_remote_run(bool start) { (void)start; return false; }

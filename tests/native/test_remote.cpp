/*
 * Native tests for src/remote_server.cpp (issue #157).
 *
 * Two defects of the control socket, both on the *read* side:
 *
 *  1. `state.pwr` frozen at 50. state_line() reported `cfg_sm.p_pwr.get()`, and
 *     Param::get() returns the in-memory `value_` (5, fixed in the constructor);
 *     the real value lives in `cfg_sm.p_pwr.getf()` -> subject_get_float(cfg.pwr.val).
 *     The command path was fine: apply_cmd's CMD_PWR does p_pwr.set(val/10.0f), which
 *     goes through the float overload (setf_) into the cfg.pwr subject. So the state
 *     line always answered "pwr":50 and could not verify the TX power setpoint.
 *
 *  2. `{"cmd":"mode","val":"NFM"}` selected SAM, and the state line read one past
 *     mode_names[] in NFM. mode_names[] had 8 entries with "NFM" at index 7, but the
 *     control enum is x6200_mode_sam = 7 / x6200_mode_nfm = 8
 *     (aether_radio/x6200_control/low/control.h). The bounds check was
 *     `mode <= x6100_mode_nfm` (8) against an 8-entry table: mode 8 read mode_names[8].
 *
 * The server under test is staged next to minimal stubs (tests/native/stubs_remote/),
 * the same pattern as the other native suites; the REAL settings_manager.cpp and
 * subjects.cpp are used, so Param::get()/getf() behave exactly like on the radio. The
 * static state_line()/parse_cmd()/apply_cmd() are visible because this file includes
 * the server source; the socket thread itself is not exercised (no radio, no LVGL).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include "test_framework.h"
}

#include "cfg/cfg.h"
#include "cfg/cfg_api.h"
#include "cfg/settings_manager.h"

/* The unit under test (its static functions are what these tests pin). */
#include "remote_server.cpp"

extern "C" void cfg_sm_init_live(void);

/* The mode table the socket must map exactly, in x6200_mode_t order (cfg/cfg_api.h). */
static const struct {
    const char *name;
    int32_t     mode;
} kModes[] = {
    {"LSB",   x6200_mode_lsb},
    {"LSB-D", x6200_mode_lsb_dig},
    {"USB",   x6200_mode_usb},
    {"USB-D", x6200_mode_usb_dig},
    {"CW",    x6200_mode_cw},
    {"CW-R",  x6200_mode_cwr},
    {"AM",    x6200_mode_am},
    {"SAM",   x6200_mode_sam},
    {"NFM",   x6200_mode_nfm},
    {"WFM",   x6200_mode_wfm},
};

/* Wire the configuration subjects and rebind cfg_sm, exactly like cfg_init() does at
 * boot (issue #30: the mirror is a global whose constructor runs before cfg exists). */
static void remote_wire(void) {
    cfg.vol.val        = subject_create_int(8);
    cfg.sql.val        = subject_create_int(0);
    cfg.nb.val         = subject_create_int(0);
    cfg.nb_level.val   = subject_create_int(0);
    cfg.nb_width.val   = subject_create_int(0);
    cfg.nr.val         = subject_create_int(0);
    cfg.nr_level.val   = subject_create_int(0);
    cfg.band_id.val    = subject_create_int(0);
    cfg.pwr.val        = subject_create_float(5.0f);   /* the radio's default, 5.0 W */
    cfg.cw_decoder.val = subject_create_int(0);

    cfg_cur.fg_freq        = subject_create_int(14074000);
    cfg_cur.bg_freq        = subject_create_int(14074000);
    cfg_cur.mode           = subject_create_int(x6200_mode_usb);
    cfg_cur.agc            = subject_create_int(x6200_agc_auto);
    cfg_cur.att            = subject_create_int(0);
    cfg_cur.pre            = subject_create_int(0);
    cfg_cur.filter.bw      = subject_create_int(2400);
    cfg_cur.filter.low     = subject_create_int(50);
    cfg_cur.filter.high    = subject_create_int(2450);
    cfg_cur.lo_offset      = subject_create_int(0);

    cfg_sm_init_live();
}

/* The power the state line reports (`"pwr":N`) must be the applied setpoint. */
static void test_pwr_state(void) {
    static const int32_t kPwr[] = {1, 5, 10, 50, 100};
    char                 buf[2560];

    for (size_t i = 0; i < sizeof(kPwr) / sizeof(kPwr[0]); i++) {
        char  json[64];
        cmd_t cmd;

        snprintf(json, sizeof(json), "{\"cmd\":\"pwr\",\"val\":%d}", kPwr[i]);
        cJSON *req = cJSON_Parse(json);
        const char *err = parse_cmd(req, "pwr", &cmd);
        CHECK(err == NULL);
        CHECK(cmd.id == CMD_PWR);
        CHECK(cmd.val == kPwr[i]);
        cJSON_Delete(req);

        apply_cmd(&cmd);

        /* The command wrote the real setpoint... */
        CHECK_CLOSE(subject_get_float(cfg.pwr.val), kPwr[i] / 10.0f, 1e-4f);
        /* ...and the state line reports it (not the frozen 50). */
        CHECK(state_line(buf, sizeof(buf)) > 0);
        char want[32];
        snprintf(want, sizeof(want), "\"pwr\":%d,", kPwr[i]);
        CHECK(strstr(buf, want) != NULL);
    }

    /* The contract of the range is unchanged. */
    cJSON *zero = cJSON_Parse("{\"cmd\":\"pwr\",\"val\":0}");
    cJSON *over = cJSON_Parse("{\"cmd\":\"pwr\",\"val\":101}");
    cmd_t  c;
    CHECK(parse_cmd(zero, "pwr", &c) != NULL);
    CHECK(parse_cmd(over, "pwr", &c) != NULL);
    cJSON_Delete(zero);
    cJSON_Delete(over);
}

/* Every supported mode: name -> enum (command) and enum -> name (state), no off-by-one. */
static void test_mode_roundtrip(void) {
    char buf[2560];

    for (size_t i = 0; i < sizeof(kModes) / sizeof(kModes[0]); i++) {
        char  json[64];
        cmd_t cmd;

        snprintf(json, sizeof(json), "{\"cmd\":\"mode\",\"val\":\"%s\"}", kModes[i].name);
        cJSON *req = cJSON_Parse(json);
        const char *err = parse_cmd(req, "mode", &cmd);
        CHECK(err == NULL);
        CHECK(cmd.id == CMD_MODE);
        CHECK(cmd.val == kModes[i].mode);   /* NFM must be 8, SAM must be 7 */
        cJSON_Delete(req);

        apply_cmd(&cmd);
        CHECK(subject_get_int(cfg_cur.mode) == kModes[i].mode);

        CHECK(state_line(buf, sizeof(buf)) > 0);
        char want[32];
        snprintf(want, sizeof(want), "\"mode\":\"%s\"", kModes[i].name);
        CHECK(strstr(buf, want) != NULL);
    }
}

/* The state line must map every enum value and never index past mode_names[]. */
static void test_mode_state_bounds(void) {
    char buf[2560];

    for (int32_t m = 0; m <= x6200_mode_wfm; m++) {
        char want[32];

        subject_set_int(cfg_cur.mode, m);
        CHECK(state_line(buf, sizeof(buf)) > 0);
        snprintf(want, sizeof(want), "\"mode\":\"%s\"", kModes[m].name);
        CHECK(strstr(buf, want) != NULL);
    }

    /* One past the table: the old `mode <= x6100_mode_nfm` check read mode_names[8]. */
    subject_set_int(cfg_cur.mode, x6200_mode_wfm + 1);
    CHECK(state_line(buf, sizeof(buf)) > 0);
    CHECK(strstr(buf, "\"mode\":\"?\"") != NULL);

    /* And below zero. */
    subject_set_int(cfg_cur.mode, -1);
    CHECK(state_line(buf, sizeof(buf)) > 0);
    CHECK(strstr(buf, "\"mode\":\"?\"") != NULL);
}

/* An unknown mode name is still rejected (the rest of the contract). */
static void test_mode_invalid(void) {
    cJSON *req = cJSON_Parse("{\"cmd\":\"mode\",\"val\":\"BOGUS\"}");
    cmd_t  cmd;
    const char *err = parse_cmd(req, "mode", &cmd);

    CHECK(err != NULL);
    CHECK(err != NULL && strcmp(err, "Invalid mode") == 0);
    cJSON_Delete(req);
}

extern "C" void test_remote(void) {
    remote_wire();

    test_pwr_state();
    test_mode_roundtrip();
    test_mode_state_bounds();
    test_mode_invalid();
}

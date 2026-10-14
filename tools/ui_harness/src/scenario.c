/*
 *  Scenario file parsing for the host UI harness (see scenario.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "scenario.h"
#include "mock_radio.h"

#include "params/params.h"

static void trim(char *s)
{
    char *p = s;

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }

    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
        s[--n] = 0;
    }
}

static bool parse_bool(const char *v, bool *out)
{
    if (!strcasecmp(v, "true") || !strcasecmp(v, "on") || !strcmp(v, "1")) {
        *out = true;
        return true;
    }
    if (!strcasecmp(v, "false") || !strcasecmp(v, "off") || !strcmp(v, "0")) {
        *out = false;
        return true;
    }
    return false;
}

/* The mode the fixture spells. The canonical names are the ones the radio itself
 * lists and accepts (`mode_names[]`, src/remote_server.cpp: LSB, LSB-D, USB,
 * USB-D, CW, CW-R, AM, NFM), so a fixture can use the same string the user would
 * send over the remote control; the enum-ish short forms (`cwr`, `usb_dig`) are
 * kept because the older fixtures use them. */
static bool parse_mode(const char *v, x6200_mode_t *out)
{
    if (!strcasecmp(v, "usb"))     { *out = x6200_mode_usb;     return true; }
    if (!strcasecmp(v, "lsb"))     { *out = x6200_mode_lsb;     return true; }
    if (!strcasecmp(v, "usb-dig") || !strcasecmp(v, "usb_dig") || !strcasecmp(v, "usb-d")) {
        *out = x6200_mode_usb_dig; return true;
    }
    if (!strcasecmp(v, "lsb-dig") || !strcasecmp(v, "lsb_dig") || !strcasecmp(v, "lsb-d")) {
        *out = x6200_mode_lsb_dig; return true;
    }
    if (!strcasecmp(v, "cw"))      { *out = x6200_mode_cw;      return true; }
    if (!strcasecmp(v, "cwr") || !strcasecmp(v, "cw-r")) { *out = x6200_mode_cwr; return true; }
    if (!strcasecmp(v, "am"))      { *out = x6200_mode_am;      return true; }
    if (!strcasecmp(v, "sam"))     { *out = x6200_mode_sam;     return true; }
    if (!strcasecmp(v, "nfm"))     { *out = x6200_mode_nfm;     return true; }
    if (!strcasecmp(v, "wfm"))     { *out = x6200_mode_wfm;     return true; }
    return false;
}

static bool parse_vfo(const char *v, x6200_vfo_t *out)
{
    if (!strcasecmp(v, "a")) { *out = X6200_VFO_A; return true; }
    if (!strcasecmp(v, "b")) { *out = X6200_VFO_B; return true; }
    return false;
}

static bool parse_agc(const char *v, x6200_agc_t *out)
{
    if (!strcasecmp(v, "off"))  { *out = x6200_agc_off;  return true; }
    if (!strcasecmp(v, "slow")) { *out = x6200_agc_slow; return true; }
    if (!strcasecmp(v, "fast")) { *out = x6200_agc_fast; return true; }
    if (!strcasecmp(v, "auto")) { *out = x6200_agc_auto; return true; }
    return false;
}

static bool apply_key(const char *key, const char *value, scenario_t *out)
{
    bool b;

    if (!strcmp(key, "name")) {
        snprintf(out->name, sizeof(out->name), "%s", value);
    } else if (!strcmp(key, "screen")) {
        snprintf(out->screen, sizeof(out->screen), "%s", value);
    } else if (!strcmp(key, "seed")) {
        out->seed = (uint32_t) strtoul(value, NULL, 10);
    } else if (!strcmp(key, "theme")) {
        snprintf(out->theme, sizeof(out->theme), "%s", value);
        params_str_set(&params.theme_name, value);
    } else if (!strcmp(key, "band_plan")) {
        return parse_bool(value, &params.band_plan.x);
    } else if (!strcmp(key, "spectrum_filled")) {
        return parse_bool(value, &params.spectrum_filled);
    } else if (!strcmp(key, "spectrum_peak")) {
        return parse_bool(value, &params.spectrum_peak);
    } else if (!strcmp(key, "waterfall_center_line")) {
        return parse_bool(value, &params.waterfall_center_line.x);
    } else if (!strcmp(key, "frequency_hz")) {
        mock_radio.frequency_hz = strtoull(value, NULL, 10);
    } else if (!strcmp(key, "tx_frequency_hz")) {
        mock_radio.tx_frequency_hz = strtoull(value, NULL, 10);
    } else if (!strcmp(key, "fft_width_hz")) {
        mock_radio.fft_width_hz = strtoull(value, NULL, 10);
    } else if (!strcmp(key, "mode")) {
        return parse_mode(value, &mock_radio.mode);
    } else if (!strcmp(key, "vfo")) {
        return parse_vfo(value, &mock_radio.vfo);
    } else if (!strcmp(key, "agc")) {
        return parse_agc(value, &mock_radio.agc);
    } else if (!strcmp(key, "split")) {
        return parse_bool(value, &mock_radio.split);
    } else if (!strcmp(key, "att")) {
        return parse_bool(value, &mock_radio.att);
    } else if (!strcmp(key, "pre")) {
        return parse_bool(value, &mock_radio.pre);
    } else if (!strcmp(key, "atu")) {
        return parse_bool(value, &mock_radio.atu_enabled);
    } else if (!strcmp(key, "ant")) {
        mock_radio.ant = (int32_t) strtol(value, NULL, 10);
    } else if (!strcmp(key, "smeter_db")) {
        mock_radio.smeter_db = (int32_t) strtol(value, NULL, 10);
    } else if (!strcmp(key, "battery_v")) {
        mock_radio.battery_v = strtof(value, NULL);
    } else if (!strcmp(key, "external_v")) {
        mock_radio.external_v = strtof(value, NULL);
    } else if (!strcmp(key, "battery_cap")) {
        mock_radio.battery_cap = (uint8_t) strtol(value, NULL, 10);
    } else if (!strcmp(key, "charging")) {
        return parse_bool(value, &b) && ((mock_radio.charging = b), true);
    } else if (!strcmp(key, "bench_warmup")) {
        long v = strtol(value, NULL, 10);

        /* Bench-only knobs: parsed here so a fixture that uses them is not an
         * "unknown key" error, but applied only in `--bench` mode. */
        if (v <= 0) { return false; }
        out->bench_warmup = (int) v;
    } else if (!strcmp(key, "bench_repeats")) {
        long v = strtol(value, NULL, 10);

        if (v <= 0) { return false; }
        out->bench_repeats = (int) v;
    } else if (!strcmp(key, "bench_redraw_passes")) {
        long v = strtol(value, NULL, 10);

        if (v <= 0) { return false; }
        out->bench_redraw_passes = (int) v;
    } else if (!strcmp(key, "natural_frames")) {
        long v = strtol(value, NULL, 10);

        /* Natural-bench-only knob: parsed here so a fixture that uses it is not
         * an "unknown key" error, but applied only in `--bench-natural` mode. */
        if (v <= 0) { return false; }
        out->natural_frames = (int) v;
    } else if (!strcmp(key, "rxtx")) {
        if (!strcasecmp(value, "rx")) { mock_radio.state = RADIO_RX; }
        else if (!strcasecmp(value, "tx")) { mock_radio.state = RADIO_TX; }
        else return false;
    } else {
        return false;       /* unknown key: refuse, never ignore */
    }
    return true;
}

bool scenario_load(const char *path, scenario_t *out)
{
    FILE *f = fopen(path, "r");

    if (!f) {
        fprintf(stderr, "scenario: cannot open %s\n", path);
        return false;
    }

    char line[512];
    int  lineno = 0;
    bool ok = true;

    while (fgets(line, sizeof(line), f)) {
        lineno++;

        char *hash = strchr(line, '#');

        if (hash) {
            *hash = 0;
        }
        trim(line);
        if (line[0] == 0) {
            continue;
        }

        char *sep = strpbrk(line, " \t=");

        if (!sep) {
            fprintf(stderr, "scenario %s:%d: expected `key value`\n", path, lineno);
            ok = false;
            break;
        }

        *sep = 0;
        char *key = line;
        char *value = sep + 1;

        trim(key);
        trim(value);

        if (!apply_key(key, value, out)) {
            fprintf(stderr, "scenario %s:%d: bad or unknown key/value: %s = %s\n",
                    path, lineno, key, value);
            ok = false;
            break;
        }
    }

    fclose(f);
    return ok;
}

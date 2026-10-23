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
 * USB-D, CW, CW-R, AM, SAM, NFM, WFM), so a fixture can use the same string the
 * user would send over the remote control; the enum-ish short forms (`cwr`,
 * `usb_dig`) are kept because the older fixtures use them. */
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
    } else if (!strcmp(key, "digits")) {
        snprintf(out->digits, sizeof(out->digits), "%s", value);
    } else if (!strcmp(key, "clock_view")) {
        /* El reloj de la barra de arriba (src/clock.c): `power` (el defecto del
         * arnes: bateria y voltios), `time` (la hora, la vista que lleva el "8
         * fantasma") o `time-power` (las dos alternandose segun los timeouts de
         * Ajustes). La hora sale del `time()` fijo del arnes (`--wrap=time`), asi
         * que la vista de hora es determinista. */
        if (!strcmp(value, "time")) {
            params.clock_view = CLOCK_TIME_ALLWAYS;
        } else if (!strcmp(value, "power")) {
            params.clock_view = CLOCK_POWER_ALLWAYS;
        } else if (!strcmp(value, "time-power")) {
            params.clock_view = CLOCK_TIME_POWER;
        } else {
            fprintf(stderr, "scenario: unknown clock_view '%s'\n", value);
            return false;
        }
    } else if (!strcmp(key, "band_plan")) {
        return parse_bool(value, &params.band_plan.x);
    } else if (!strcmp(key, "spectrum_filled")) {
        return parse_bool(value, &params.spectrum_filled);
    } else if (!strcmp(key, "spectrum_peak")) {
        return parse_bool(value, &params.spectrum_peak);
    } else if (!strcmp(key, "waterfall_center_line")) {
        return parse_bool(value, &params.waterfall_center_line.x);
    } else if (!strcmp(key, "waterfall_battery")) {
        /* The user's waterfall battery mode (the Ajustes "Waterfall battery mode"
         * switch, src/dialog_settings.c). Off by default; a fixture that turns it
         * on renders the switch checked and drives the real waterfall into its
         * saving cadence through the visual policy, exactly as the radio does. */
        return parse_bool(value, &params.waterfall_battery.x);
    } else if (!strcmp(key, "play_gain_db") || !strcmp(key, "rec_gain_db")) {
        /* Las dos ganancias de Ajustes -> Audio (los sliders de la fila "Play,Rec gain",
         * src/dialog_settings.c). El fixture les da valores distintos para que el slider de
         * cada una se pueda identificar en el render; en la radio los carga del DB con
         * params_load_float() (src/params/params.c), que escribe el mismo `.x`. */
        float *destino = (key[0] == 'p') ? &params.play_gain_db_f.x : &params.rec_gain_db_f.x;

        *destino = strtof(value, NULL);
    } else if (!strcmp(key, "frequency_hz")) {
        mock_radio.frequency_hz = strtoull(value, NULL, 10);
    } else if (!strcmp(key, "tx_frequency_hz")) {
        mock_radio.tx_frequency_hz = strtoull(value, NULL, 10);
    } else if (!strcmp(key, "fft_width_hz")) {
        mock_radio.fft_width_hz = strtoull(value, NULL, 10);
    } else if (!strcmp(key, "mode")) {
        return parse_mode(value, &mock_radio.mode);
    } else if (!strcmp(key, "filter_low")) {
        /* Filtro DFL (issue #173, PR 1): low/high del modo. El arnes calcula los
         * bordes reales de RF con el mapeo de src/cfg/mode.c::update_real_filters()
         * (mocks/mock_radio.c), asi la banda del espectro es la de la radio. */
        mock_radio.filter_low = (int32_t) strtol(value, NULL, 10);
        mock_radio.filter_set = true;
    } else if (!strcmp(key, "filter_high")) {
        mock_radio.filter_high = (int32_t) strtol(value, NULL, 10);
        mock_radio.filter_set  = true;
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
    } else if (!strcmp(key, "apps_page")) {
        /* The APP page an `apps-menu` fixture opens (1..6). The fixture name and
         * this key pick one of the six pages of `buttons_group_app`; out of range
         * is refused, never silently clamped (a typo would render page 1 and look
         * green). */
        long v = strtol(value, NULL, 10);

        if (v < 1 || v > 6) { return false; }
        out->apps_page = (int) v;
    } else if (!strcmp(key, "settings_grupo")) {
        /* El grupo de Ajustes que el fixture `settings-grupo` abre, por el nombre
         * del tab de la barra de abajo (src/dialog_settings.c, tabs_items). El
         * fixture lo valida contra la tabla de screens.c: un nombre que no es uno
         * de los ocho hace que el arnes corte, nunca que capture otro grupo. */
        snprintf(out->settings_grupo, sizeof(out->settings_grupo), "%s", value);
    } else if (!strcmp(key, "swr_dataset")) {
        /* SWR Scan (issue #125): the deterministic dataset of
         * tests/fixtures/swrscan/ the fake ROE acquisition backend replays through
         * the real dialog. The backend derives the grid (centre and span) from the
         * dataset itself, so a fixture cannot ask for a sweep that does not match
         * its data; an empty value means "no dataset", which the `swrscan` screen
         * refuses (never a green capture of an empty sweep). */
        snprintf(out->swr_dataset, sizeof(out->swr_dataset), "%s", value);
    } else if (!strcmp(key, "swr_passes")) {
        /* 1 = Quick (the only one the shipping ROM enables), 3 = Stable, 5 = Fine.
         * Out of range is refused: the dialog's gate is what decides whether the
         * session runs, and a typo must not silently become 1 pass. */
        long v = strtol(value, NULL, 10);

        if (v != 1 && v != 3 && v != 5) { return false; }
        out->swr_passes = (int) v;
    } else if (!strcmp(key, "swr_feed")) {
        /* Samples to feed before the backend stops (0 = the whole session). */
        long v = strtol(value, NULL, 10);

        if (v < 0) { return false; }
        out->swr_feed = (int) v;
    } else if (!strcmp(key, "swr_stop")) {
        return parse_bool(value, &out->swr_stop);
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

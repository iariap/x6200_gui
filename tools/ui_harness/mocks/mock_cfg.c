/*
 *  Minimal live configuration for the host UI harness.
 *
 *  The firmware keeps the running configuration in SQLite (src/cfg/). The
 *  harness does not open a database: it creates the same cfg_t / cfg_cur_t
 *  globals and the subjects the rendered components read, with the values a
 *  20 m USB session would have. Anything the components can call is answered
 *  here with an explicit value, never with an unvalidated zero.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>
#include <string.h>

#include "cfg/cfg.h"
#include "cfg/memory.h"
#include "cfg/mode.h"
#include "cfg/subjects.h"
#include "cfg/transverter.h"
#include "mock_harness.h"
#include "mock_radio.h"

cfg_t      cfg;
cfg_cur_t  cfg_cur;

static atu_network_t atu_network;
static cfg_band_t    band0;

/* cfg_transverters is a real array in the firmware; the defaults are the ones
 * src/cfg/transverter.c:36-50 creates before loading the DB (MHz -> Hz are the
 * values Ajustes shows after dividing by 1000000). */
cfg_transverter_t cfg_transverters[TRANSVERTER_NUM];

static const int32_t dflt_from[TRANSVERTER_NUM]  = { 70000000, 144000000, 432000000 };
static const int32_t dflt_to[TRANSVERTER_NUM]    = { 72000000, 150000000, 438000000 };
static const int32_t dflt_shift[TRANSVERTER_NUM] = { 42000000, 116000000, 404000000 };

void mock_cfg_init(void)
{
    cfg_cur.fg_freq   = subject_create_int(14074000);
    cfg_cur.bg_freq   = subject_create_int(14074000);
    cfg_cur.lo_offset = subject_create_int(0);
    cfg_cur.mode      = subject_create_int(x6200_mode_usb);
    cfg_cur.agc       = subject_create_int(x6200_agc_auto);
    cfg_cur.att       = subject_create_int(0);
    cfg_cur.pre       = subject_create_int(0);
    cfg_cur.freq_step = subject_create_int(100);
    cfg_cur.zoom      = subject_create_int(0);
    cfg_cur.fft_width = subject_create_int(FFT_FULL_WIDTH);
    cfg_cur.sql_level = subject_create_int(0);

    /* Filter edges: spectrum.c/subscribe to the "real" passband. */
    cfg_cur.filter.low      = subject_create_int(0);
    cfg_cur.filter.high     = subject_create_int(3000);
    cfg_cur.filter.bw       = subject_create_int(3000);
    cfg_cur.filter.real.from = subject_create_int(0);
    cfg_cur.filter.real.to   = subject_create_int(3000);

    atu_network.loaded  = subject_create_int(0);
    atu_network.network = subject_create_int(0);
    cfg_cur.atu = &atu_network;

    band0.vfo.val   = subject_create_int(X6200_VFO_A);
    band0.split.val = subject_create_int(0);
    band0.rfg.val   = subject_create_int(100);   /* the button bar shows RF gain */
    band0.vfo_a.freq.val = subject_create_int(14074000);
    band0.vfo_a.mode.val = subject_create_int(x6200_mode_usb);
    band0.vfo_b.freq.val = subject_create_int(14074000);
    band0.vfo_b.mode.val = subject_create_int(x6200_mode_usb);
    /* Spectrum/waterfall vertical scale (dB): the real values come from the DB. */
    band0.grid.min.val = subject_create_int(-120);
    band0.grid.max.val = subject_create_int(-40);
    cfg_cur.band = &band0;

    cfg.atu_enabled.val = subject_create_int(0);
    cfg.ant_id.val      = subject_create_int(1);
    cfg.vol.val         = subject_create_int(50);
    cfg.pwr.val         = subject_create_float(5.0f);
    cfg.nr.val          = subject_create_int(0);
    cfg.nb.val          = subject_create_int(0);

    /* DSP state spectrum.c watches, and the CW switches pannel/cw_tune read. */
    cfg.dnf.val        = subject_create_int(x6200_dnf_off);
    cfg.dnf_center.val = subject_create_int(0);
    cfg.dnf_width.val  = subject_create_int(100);
    cfg.cw_decoder.val = subject_create_int(0);
    cfg.cw_tune.val    = subject_create_int(0);

    /* Ajustes (src/dialog_settings.c) reads the cluster column switch, which is
     * a cfg item and not a params_* (the real default is off, src/cfg/cfg.c:323). */
    cfg.cluster_column.val = subject_create_int(0);

    /* The transverter rows of Ajustes read three subjects each; src/cfg/
     * transverter.c:36-50 creates them with these same defaults (and would load
     * them from the DB, which the harness has none of). Without them the dialog
     * would dereference a NULL subject. */
    for (uint8_t n = 0; n < TRANSVERTER_NUM; n++) {
        cfg_transverters[n].from.val  = subject_create_int(dflt_from[n]);
        cfg_transverters[n].to.val    = subject_create_int(dflt_to[n]);
        cfg_transverters[n].shift.val = subject_create_int(dflt_shift[n]);
    }
}

/* --- helpers the rendered components call ---------------------------------- */

const char *cfg_mode_agc_label(x6200_agc_t val)
{
    switch (val) {
        case x6200_agc_off:  return "off";
        case x6200_agc_slow: return "slow";
        case x6200_agc_fast: return "fast";
        default:             return "auto";
    }
}

int32_t cfg_transverter_get_shift(int32_t freq)
{
    (void) freq;
    return 0;
}

int cfg_atu_save_network(uint32_t value)
{
    (void) value;
    return 0;
}

/* Band/memory/mode helpers: the harness owns no database, so the calls that
 * would touch it answer with the harness state (a fixed step, no memory). */

void cfg_band_vfo_copy(void) {}
void cfg_band_load_next(bool up) { (void) up; }

bool cfg_memory_load(int32_t id) { (void) id; return false; }
void cfg_memory_save(int32_t id) { (void) id; }

int32_t cfg_mode_change_freq_step(bool up) { (void) up; return 100; }
int32_t cfg_mode_set_low_filter(int32_t val) { return val; }
int32_t cfg_mode_set_high_filter(int32_t val) { return val; }

const char *cfg_dnf_label_get(void) { return "Off"; }

/* The IARU Region 1 band-plan rows band_info/band_plan draw. The real rows
 * come from the SQLite catalogue; this is the same public plan (see
 * src/band_plan_data.c), enough for the strip to show the right band whatever
 * the fixture tunes to. */
uint32_t cfg_band_read_all_bands(band_info_t **results, int32_t *cap)
{
    static const struct {
        const char *name;
        uint32_t    start;
        uint32_t    stop;
    } segs[] = {
        { "CW",  1810000,  1838000 }, { "DIG", 1838000,  1843000 }, { "SSB", 1843000,  2000000 },
        { "CW",  3500000,  3570000 }, { "DIG", 3570000,  3600000 }, { "SSB", 3600000,  3800000 },
        { "CW",  7000000,  7040000 }, { "DIG", 7040000,  7050000 }, { "SSB", 7050000,  7200000 },
        { "CW", 10100000, 10130000 }, { "DIG", 10130000, 10150000 },
        { "CW", 14000000, 14070000 }, { "DIG", 14070000, 14099000 }, { "BCN", 14099000, 14101000 },
        { "DIG", 14101000, 14112000 }, { "SSB", 14112000, 14350000 },
        { "CW", 18068000, 18095000 }, { "DIG", 18095000, 18109000 }, { "BCN", 18109000, 18111000 },
        { "DIG", 18111000, 18120000 }, { "SSB", 18120000, 18168000 },
        { "CW", 21000000, 21070000 }, { "DIG", 21070000, 21120000 }, { "CW", 21120000, 21149000 },
        { "BCN", 21149000, 21151000 }, { "SSB", 21151000, 21450000 },
        { "CW", 24890000, 24915000 }, { "DIG", 24915000, 24929000 }, { "BCN", 24929000, 24931000 },
        { "SSB", 24931000, 24990000 },
        { "CW", 28000000, 28070000 }, { "DIG", 28070000, 28190000 }, { "BCN", 28190000, 28225000 },
        { "SSB", 28225000, 29700000 },
        { "CW", 50000000, 50100000 }, { "SSB", 50100000, 50300000 }, { "DIG", 50300000, 50400000 },
        { "BCN", 50400000, 50500000 }, { "SSB", 50500000, 52000000 },
    };
    size_t n = sizeof(segs) / sizeof(segs[0]);

    if (*results == NULL || *cap < (int32_t) n) {
        *results = realloc(*results, sizeof(**results) * n);
        *cap = (int32_t) n;
    }

    for (size_t i = 0; i < n; i++) {
        (*results)[i].id         = (int32_t) i;
        (*results)[i].name       = strdup(segs[i].name);
        (*results)[i].start_freq = segs[i].start;
        (*results)[i].stop_freq  = segs[i].stop;
        (*results)[i].active     = 1;
    }
    return (uint32_t) n;
}

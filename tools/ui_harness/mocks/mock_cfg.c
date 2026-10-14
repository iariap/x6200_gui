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
#include "cfg/subjects.h"
#include "cfg/transverter.h"
#include "mock_harness.h"
#include "mock_radio.h"

cfg_t      cfg;
cfg_cur_t  cfg_cur;

static atu_network_t atu_network;
static cfg_band_t    band0;

/* cfg_transverters is a real array in the firmware; empty here (no shift). */
cfg_transverter_t cfg_transverters[TRANSVERTER_NUM];

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

    atu_network.loaded  = subject_create_int(0);
    atu_network.network = subject_create_int(0);
    cfg_cur.atu = &atu_network;

    band0.vfo.val   = subject_create_int(X6200_VFO_A);
    band0.split.val = subject_create_int(0);
    band0.vfo_a.freq.val = subject_create_int(14074000);
    band0.vfo_a.mode.val = subject_create_int(x6200_mode_usb);
    cfg_cur.band = &band0;

    cfg.atu_enabled.val = subject_create_int(0);
    cfg.ant_id.val      = subject_create_int(1);
    cfg.nr.val          = subject_create_int(0);
    cfg.nb.val          = subject_create_int(0);
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

/* The IARU Region 1 band plan rows the band-plan/band-info strips draw. A
 * short, fixed 20 m set: enough for a deterministic strip and nothing more. */
uint32_t cfg_band_read_all_bands(band_info_t **results, int32_t *cap)
{
    static const struct {
        const char *name;
        uint32_t    start;
        uint32_t    stop;
    } segs[] = {
        { "CW",  14000000, 14070000 },
        { "DIG", 14070000, 14099000 },
        { "SSB", 14101000, 14350000 },
        { "BCN", 14100000, 14101000 },
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

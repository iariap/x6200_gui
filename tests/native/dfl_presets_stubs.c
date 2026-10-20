/*
 * Stubs del test de presets del DFL (tests/native/test_dfl_presets.c).
 *
 * El Subject es un entero en memoria y los dos setters son el ESPEJO EXACTO de
 * src/cfg/mode.c::cfg_mode_set_low_filter()/cfg_mode_set_high_filter(), incluidas
 * las guardas y el `return 0` de LOW en AM/SAM/NFM/WFM. Ademas replican los
 * observers de mode.c que derivan el BW (on_cur_filter_low/high_change), para
 * poder afirmar que el BW sale de los limites y que NADIE escribe
 * cfg_cur.filter.bw desde el preset.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <sqlite3.h>
#include <stdio.h>

#include "cfg/cfg.h"
#include "cfg/mode.h"

#include "dfl_presets_probe.h"

struct Subject {
    int32_t v;
};

static Subject s_mode;
static Subject s_low;
static Subject s_high;
static Subject s_bw;
static Subject s_real_from;
static Subject s_real_to;

cfg_cur_t cfg_cur;
sqlite3 *db = NULL;

static int bw_writes;
static int real_writes;

void dfl_pt_reset(int32_t mode, int32_t low, int32_t high) {
    s_mode.v      = mode;
    s_low.v       = low;
    s_high.v      = high;
    s_bw.v        = high - low;
    s_real_from.v = 0;
    s_real_to.v   = 0;

    cfg_cur.mode              = &s_mode;
    cfg_cur.filter.low        = &s_low;
    cfg_cur.filter.high       = &s_high;
    cfg_cur.filter.bw         = &s_bw;
    cfg_cur.filter.real.from  = &s_real_from;
    cfg_cur.filter.real.to    = &s_real_to;

    bw_writes   = 0;
    real_writes = 0;
}

int32_t dfl_pt_low(void) { return s_low.v; }
int32_t dfl_pt_high(void) { return s_high.v; }
int32_t dfl_pt_bw(void) { return s_bw.v; }

int dfl_pt_bw_writes(void) { return bw_writes; }
int dfl_pt_real_writes(void) { return real_writes; }

int32_t subject_get_int(Subject *subj) {
    return subj->v;
}

void subject_set_int(Subject *subj, int32_t val) {
    if (subj == &s_bw) {
        bw_writes++;
    }
    if (subj == &s_real_from || subj == &s_real_to) {
        real_writes++;
    }
    subj->v = val;
}

/* Espejo de los observers de src/cfg/mode.c (solo el BW: el par real RF no se
 * toca en este test). */
static void derive_bw(void) {
    s_bw.v = s_high.v - s_low.v;
}

int32_t cfg_mode_set_low_filter(int32_t val) {
    x6200_mode_t mode = (x6200_mode_t) subject_get_int(cfg_cur.mode);
    int32_t      high = subject_get_int(cfg_cur.filter.high);

    switch (mode) {
        case x6200_mode_am:
        case x6200_mode_sam:
        case x6200_mode_nfm:
        case x6200_mode_wfm:
            return 0;
        case x6200_mode_cw:
        case x6200_mode_cwr:
            if (val < high) {
                subject_set_int(cfg_cur.filter.low, val);
                derive_bw();
            }
            break;
        default:
            if ((val >= 0) & (val < high)) {
                subject_set_int(cfg_cur.filter.low, val);
                derive_bw();
            }
    }

    return subject_get_int(cfg_cur.filter.low);
}

int32_t cfg_mode_set_high_filter(int32_t val) {
    int32_t low = subject_get_int(cfg_cur.filter.low);

    if ((val <= MAX_FILTER_FREQ) & (val > low)) {
        subject_set_int(cfg_cur.filter.high, val);
        derive_bw();
    }

    return subject_get_int(cfg_cur.filter.high);
}

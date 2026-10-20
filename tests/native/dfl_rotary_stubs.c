/*
 * Stubs del test del motor de la rueda del editor DFL (tests/native/test_dfl_rotary.c).
 *
 * El Subject es un entero en memoria y los setters son el ESPEJO EXACTO de
 * src/cfg/mode.c::cfg_mode_set_low_filter()/cfg_mode_set_high_filter(): mismas
 * guardas, incluido el `return 0` de LOW en AM/SAM/NFM/WFM. Asi el test
 * ejercita la misma decision que la radio, sin LVGL y sin la DB.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "cfg/cfg.h"
#include "cfg/mode.h"

#include "dfl_rotary_probe.h"

#include <stdarg.h>
#include <stdio.h>

/* El Subject del stub: un entero en memoria (el resto de subjects.h es real). */
struct Subject {
    int32_t v;
};

static Subject s_mode;
static Subject s_low;
static Subject s_high;
static Subject s_bw;

cfg_cur_t cfg_cur;

static char last_msg[128];

void dfl_test_reset(int32_t mode, int32_t low, int32_t high, int32_t bw) {
    s_mode.v = mode;
    s_low.v  = low;
    s_high.v = high;
    s_bw.v   = bw;

    cfg_cur.mode        = &s_mode;
    cfg_cur.filter.low  = &s_low;
    cfg_cur.filter.high = &s_high;
    cfg_cur.filter.bw   = &s_bw;

    last_msg[0] = 0;
}

int32_t dfl_test_low(void) {
    return s_low.v;
}

int32_t dfl_test_high(void) {
    return s_high.v;
}

int32_t dfl_test_bw(void) {
    return s_bw.v;
}

int32_t subject_get_int(Subject *subj) {
    return subj->v;
}

void subject_set_int(Subject *subj, int32_t val) {
    subj->v = val;
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
            }
            break;
        default:
            if ((val >= 0) & (val < high)) {
                subject_set_int(cfg_cur.filter.low, val);
            }
    }

    return subject_get_int(cfg_cur.filter.low);
}

int32_t cfg_mode_set_high_filter(int32_t val) {
    int32_t low = subject_get_int(cfg_cur.filter.low);

    if ((val <= MAX_FILTER_FREQ) & (val > low)) {
        subject_set_int(cfg_cur.filter.high, val);
    }

    return subject_get_int(cfg_cur.filter.high);
}

void msg_update_text_fmt(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vsnprintf(last_msg, sizeof(last_msg), fmt, args);
    va_end(args);
}

const char *dfl_test_last_msg(void) {
    return last_msg;
}

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Aplicacion de los cambios del editor DFL (ver src/dfl_rotary.h).
 */

#include "dfl_rotary.h"

#include "cfg/cfg.h"
#include "cfg/mode.h"
#include "cfg/subjects.h"
#include "msg.h"

int32_t dfl_set_low(int32_t val) {
    cfg_mode_set_low_filter(val);
    return subject_get_int(cfg_cur.filter.low);
}

int32_t dfl_set_high(int32_t val) {
    cfg_mode_set_high_filter(val);
    return subject_get_int(cfg_cur.filter.high);
}

/* BW: el par LOW/HIGH resultante se calcula con el espejo de
 * on_cur_filter_bw_change() y se valida ANTES de escribir. Si no es legal se
 * bloquea la accion y se conserva el Subject (no queda valor optimista). */
int32_t dfl_set_bw(int32_t val) {
    if (val < dfl_step_bw()) {
        return subject_get_int(cfg_cur.filter.bw);
    }

    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    int32_t      low  = subject_get_int(cfg_cur.filter.low);
    int32_t      high = subject_get_int(cfg_cur.filter.high);
    int32_t      new_low;
    int32_t      new_high;

    dfl_bw_targets(mode, low, high, val, &new_low, &new_high);

    if (!dfl_bw_legal(mode, new_low, new_high)) {
        return subject_get_int(cfg_cur.filter.bw);
    }

    subject_set_int(cfg_cur.filter.bw, val);
    return subject_get_int(cfg_cur.filter.bw);
}

static int32_t rotary_low(int32_t diff, x6200_mode_t mode) {
    if (!dfl_mode_low_editable(mode)) {
        return subject_get_int(cfg_cur.filter.low);
    }

    int32_t step = dfl_step_low(mode);
    int32_t val  = dfl_quantize(subject_get_int(cfg_cur.filter.low) + diff * step, step);

    return dfl_set_low(val);
}

static int32_t rotary_high(int32_t diff, x6200_mode_t mode) {
    int32_t step = dfl_step_high(mode);
    int32_t val  = dfl_quantize(subject_get_int(cfg_cur.filter.high) + diff * step, step);

    return dfl_set_high(val);
}

static int32_t rotary_bw(int32_t diff) {
    int32_t step = dfl_step_bw();
    int32_t val  = dfl_quantize(subject_get_int(cfg_cur.filter.bw) + diff * step, step);

    return dfl_set_bw(val);
}

int32_t dfl_rotary_apply(dfl_sel_t sel, int32_t diff) {
    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    int32_t      v    = 0;

    switch (sel) {
        case DFL_SEL_LOW:
            v = rotary_low(diff, mode);
            msg_update_text_fmt("DFL LOW %d Hz", (int) v);
            break;
        case DFL_SEL_HIGH:
            v = rotary_high(diff, mode);
            msg_update_text_fmt("DFL HIGH %d Hz", (int) v);
            break;
        case DFL_SEL_BW:
            v = rotary_bw(diff);
            msg_update_text_fmt("DFL BW %d Hz", (int) v);
            break;
        default:
            break;
    }

    return v;
}

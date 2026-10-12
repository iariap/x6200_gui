/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - radio-state handles for the ported app code.
 *
 *  The X6100 reads the live radio state as cfg_* handles (its configuration
 *  items) through param_i_get()/param_f_get()/param_i_set(). This gives the
 *  ported WSPR app the same spelling on top of this GUI's cfg_cur subjects.
 */

#pragma once

#include "cfg.h"
#include "common.h"
#include "../params/params.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Live radio state */
#define cfg_fg_freq         (cfg_cur.fg_freq)
#define cfg_bg_freq         (cfg_cur.bg_freq)
#define cfg_cur_mode        (cfg_cur.mode)
#define cfg_pwr             (&cfg.pwr)
#define cfg_band_split      (cfg_cur.band->split.val)

/* WSPR settings (persisted in the params table) */
#define cfg_wspr_tx_freq    (&cfg.wspr_tx_freq)
#define cfg_wspr_tx_pct     (&cfg.wspr_tx_pct)
#define cfg_wspr_upload     (&cfg.wspr_upload)

static inline int32_t cfg_api_int_get(const cfg_item_t *item) {
    return subject_get_int(item->val);
}

static inline void cfg_api_int_set(const cfg_item_t *item, int32_t v) {
    subject_set_int(item->val, v);
}

/* The X6100 renamed the subject accessors and the parameter getters */
#define subject_i_create(v)     subject_create_int((int32_t) (v))
#define subject_i_get(subj)     subject_get_int(subj)
#define subject_i_set(subj, v)  subject_set_int((subj), (v))
#define cparam_i_get(subj)      subject_get_int(subj)
#define subject_subscribe_delayed(subj, fn, ud) subject_add_delayed_observer((subj), (fn), (ud))

#define param_i_get(item)       cfg_api_int_get(item)
#define param_f_get(item)       ((float) cfg_api_int_get(item))
#define param_i_set(item, val)  cfg_api_int_set((item), (val))

#ifdef __cplusplus
}
#endif

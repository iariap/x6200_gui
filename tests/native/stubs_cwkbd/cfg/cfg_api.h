/*
 * Minimal cfg_api stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c calls cparam_i_get(cfg_cur_mode), param_i_get(cfg_key_speed)
 * and param_i_set(cfg_cw_decoder, ...). The accessors map to subject_get_int /
 * subject_set_int, provided by tests/native/cwkbd_stubs.c.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "cfg.h"

#define cfg_cur_mode    (cfg_cur.mode)
#define cfg_key_speed   (&cfg.key_speed)
#define cfg_cw_decoder  (&cfg.cw_decoder)

int32_t subject_get_int(Subject *subj);
void    subject_set_int(Subject *subj, int32_t v);

#define cparam_i_get(subj)   subject_get_int(subj)
#define param_i_get(item)    subject_get_int((item)->val)
#define param_i_set(item, v) subject_set_int((item)->val, (v))

/* The mode enum the firmware compares against (aether_radio's x6200_mode_t,
 * aliased to the ported x6100_* spelling). Only equality is used here. */
#define x6100_mode_cw   4
#define x6100_mode_cwr  5

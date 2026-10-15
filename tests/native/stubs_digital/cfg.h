/*
 * Minimal cfg surface for the native digital_modes test: src/cfg/digital_modes.c
 * reads cfg_cur.fg_freq and writes cfg_cur.fg_freq / cfg_cur.mode through
 * subject_get_int() / subject_set_int(). The real cfg.h pulls in common.h,
 * atu.h and band.h (and the whole cfg_t of the firmware); none of that is
 * needed here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct Subject Subject;

typedef struct {
    Subject * fg_freq;
    Subject * mode;
} cfg_cur_t;

extern cfg_cur_t cfg_cur;

int32_t subject_get_int(Subject * subj);
void subject_set_int(Subject * subj, int32_t val);

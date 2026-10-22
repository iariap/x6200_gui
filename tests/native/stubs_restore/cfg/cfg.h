/*
 * Minimal cfg stub for the native dialog restore test (issue #94).
 *
 * It keeps only the radio-state handles src/dialog.c and the stubbed
 * radio_state_save/restore read: the foreground frequency, the mode and the
 * agc/pre/att (the snapshot radio.c keeps) plus the power.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "subjects.h"

typedef struct {
    const char *db_name;
    int         pk;
    Subject    *val;
} cfg_item_t;

typedef struct {
    Subject *fg_freq;
    Subject *mode;
    Subject *agc;
    Subject *pre;
    Subject *att;
} cfg_cur_t;

typedef struct {
    cfg_item_t pwr;
} cfg_t;

extern cfg_cur_t cfg_cur;
extern cfg_t     cfg;

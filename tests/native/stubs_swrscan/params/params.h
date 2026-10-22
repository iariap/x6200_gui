/*
 * Minimal params/cfg stub for the native SWR Scan dialog test (issue #85).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "lvgl/lvgl.h"

typedef struct Subject         Subject;
typedef struct ObserverDelayed ObserverDelayed;

typedef struct {
    Subject *val;
} cfg_item_t;

typedef struct {
    cfg_item_t swrscan_linear;
    cfg_item_t swrscan_span;
    cfg_item_t pwr;
} cfg_t;

typedef struct {
    Subject *fg_freq;
    Subject *mode;
} cfg_cur_t;

extern cfg_t     cfg;
extern cfg_cur_t cfg_cur;

int32_t subject_get_int(Subject *subj);
void    subject_set_int(Subject *subj, int32_t val);

ObserverDelayed *subject_add_delayed_observer(Subject *subj, void (*fn)(Subject *, void *), void *user_data);
ObserverDelayed *subject_add_delayed_observer_and_call(Subject *subj, void (*fn)(Subject *, void *), void *user_data);
void             observer_delayed_del(ObserverDelayed *observer);

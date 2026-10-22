/*
 * cfg/cfg.h stub for the native spectrum test (issue #149).
 *
 * src/spectrum.c reads the current band grid, the FFT width, the real filter
 * edges, the mode, the frequency and the three DNF values through Subject
 * pointers, and subscribes to them. The stub keeps the same shape (cfg_cur /
 * cfg with Subject * members) with a minimal, test-owned implementation in
 * tests/native/spectrum_stubs.c: the subjects hold an int32 value, a set
 * notifies the observers in the calling thread (like the firmware's
 * SubjectT::set) and the test can change them from another thread.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "../lvgl/lvgl.h"

typedef struct Subject Subject;

typedef void (*spectrum_observer_fn_t)(Subject *subj, void *user_data);

int32_t  subject_get_int(Subject *subj);
Subject *subject_add_observer_and_call(Subject *subj, spectrum_observer_fn_t fn, void *user_data);

typedef struct {
    Subject *val;
} cfg_item_t;

typedef struct {
    struct {
        cfg_item_t min;
        cfg_item_t max;
    } grid;
} cfg_band_t;

typedef struct {
    Subject *fg_freq;
    Subject *mode;
    Subject *fft_width;
    struct {
        Subject *bw;
        struct {
            Subject *from;
            Subject *to;
        } real;
    } filter;
    cfg_band_t *band;
} cfg_cur_t;

typedef struct {
    cfg_item_t dnf;
    cfg_item_t dnf_center;
    cfg_item_t dnf_width;
} cfg_t;

extern cfg_cur_t cfg_cur;
extern cfg_t     cfg;

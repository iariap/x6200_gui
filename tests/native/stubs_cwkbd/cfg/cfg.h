/*
 * Minimal cfg stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c reads cfg_cur.mode (cw_mode()), cfg.key_speed (the WPM in
 * the header) and writes cfg.cw_decoder (the web remote's `cw_dec`). cfg_item_t
 * keeps the REAL member it touches (val, a Subject*); the rest of the real cfg_t
 * is not needed here. The Subject backing is in tests/native/cwkbd_stubs.c.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef struct Subject Subject;

typedef struct cfg_item_t {
    Subject *val;
} cfg_item_t;

typedef struct {
    cfg_item_t key_speed;
    cfg_item_t key_ratio;
    cfg_item_t key_train;
    cfg_item_t cw_decoder;
} cfg_t;

extern cfg_t cfg;

/* Live radio state: the dialog reads cfg_cur.mode (CW/CWR vs the rest). */
typedef struct {
    Subject *mode;
    Subject *fg_freq;
} cfg_cur_t;

extern cfg_cur_t cfg_cur;

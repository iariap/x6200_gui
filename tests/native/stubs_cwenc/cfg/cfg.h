/*
 * Minimal cfg stub for the native CW encoder test (issue #116).
 *
 * src/cw_encoder.c reads only cfg.key_speed.val, cfg.key_ratio.val and
 * cfg.key_train.val (through cfg_api.h). cfg_item_t keeps the REAL member it
 * touches (val, a Subject*); the rest of the real cfg_t is not needed here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef struct Subject Subject;

typedef struct cfg_item_t {
    Subject *val;
} cfg_item_t;

typedef struct {
    cfg_item_t key_speed;
    cfg_item_t key_ratio;
    cfg_item_t key_train;
} cfg_t;

extern cfg_t cfg;

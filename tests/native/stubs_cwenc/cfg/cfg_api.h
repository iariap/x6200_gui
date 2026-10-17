/*
 * Minimal cfg_api stub for the native CW encoder test (issue #116).
 *
 * src/cw_encoder.c uses subject_get_int(cfg.key_speed.val),
 * subject_get_float(cfg.key_ratio.val) and param_i_get(cfg_key_train). The
 * accessors are provided by tests/native/cwenc_stubs.c.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "cfg.h"

#define cfg_key_train   (&cfg.key_train)

int32_t subject_get_int(Subject *subj);
float   subject_get_float(Subject *subj);

#define param_i_get(item)   subject_get_int((item)->val)

/*
 * params/params.h stub for the native spectrum test (issue #149).
 *
 * Only the parameters src/spectrum.c actually reads: the peak/filled display
 * switches, the peak hold/decay and the RTTY markers. The struct is a plain
 * global the test sets directly (see tests/native/test_spectrum.c).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../cfg/cfg.h"

typedef struct {
    float x;
} params_bool_t;

typedef struct {
    bool          spectrum_peak;
    uint16_t      spectrum_peak_hold;
    float         spectrum_peak_speed;
    bool          spectrum_filled;
    params_bool_t spectrum_auto_min;
    params_bool_t spectrum_auto_max;
    uint16_t      rtty_center;
    uint16_t      rtty_shift;
} params_t;

extern params_t params;

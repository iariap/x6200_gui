/*
 * Minimal params stub for the native CW encoder test (issue #116).
 *
 * src/cw_encoder.c reads params.cw_encoder_period only (the beacon pause).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef struct {
    uint16_t cw_encoder_period;   /* seconds */
} params_t;

extern params_t params;

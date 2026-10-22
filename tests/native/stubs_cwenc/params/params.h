/*
 * Minimal params stub for the native CW encoder test (issue #116).
 *
 * src/cw_encoder.c reads params.cw_encoder_period (the beacon pause) and
 * params.cb_tx_scope.x (the CB TX interlock, issue #160).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

/* Mirror of the real params_uint8_t: the member the keyer reads is `.x`. */
typedef struct {
    uint8_t x;
} params_uint8_t;

typedef struct {
    uint16_t      cw_encoder_period;   /* seconds */
    params_uint8_t cb_tx_scope;        /* 0 = solo SBC AR, 1 = grids A-D, 2 = off */
} params_t;

extern params_t params;

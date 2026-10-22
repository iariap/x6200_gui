/*
 * Minimal radio stub for the native SWR Scan dialog test (issue #85).
 *
 * The test stubs (tests/native/swrscan_stubs.c) record these calls and make
 * radio_start_swrscan() succeed, so the dialog reaches its "run" state.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* #133: el comando remoto no puede arrancar el barrido sobre una TX ajena. El test mueve
 * este estado (swrscan_stub_set_radio_state) para pinnear las guardas. */
typedef enum {
    RADIO_RX = 0,
    RADIO_TX,
    RADIO_ATU_START,
    RADIO_ATU_WAIT,
    RADIO_ATU_RUN,
    RADIO_SWRSCAN,

    RADIO_POWEROFF,
    RADIO_OFF
} radio_state_t;

radio_state_t radio_get_state(void);

void     radio_set_freq(int32_t freq);
uint16_t radio_change_vol(int16_t df);
bool     radio_start_swrscan(void);
void     radio_stop_swrscan(void);

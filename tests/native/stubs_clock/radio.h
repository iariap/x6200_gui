/*
 * Minimal radio.h for the native clock test: only the RX state clock.c reads.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

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

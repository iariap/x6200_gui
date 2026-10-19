/* Stub for src/radio.h (native remote_server test). */
#pragma once

#include <stdint.h>
#include <stdbool.h>

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

radio_state_t radio_get_state();
uint16_t      radio_change_vol(int16_t df);

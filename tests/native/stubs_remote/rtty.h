/* Stub for src/rtty.h (native remote_server test). */
#pragma once

typedef enum {
    RTTY_OFF = 0,
    RTTY_RX,
    RTTY_TX
} rtty_state_t;

rtty_state_t rtty_get_state();

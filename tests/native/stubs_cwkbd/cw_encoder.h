/*
 * Minimal cw_encoder stub header for the native CW keyboard dialog test (#217).
 *
 * src/dialog_cw_kbd.c drives the keyer (send/stop/state/sent). The stub
 * implementation in tests/native/cwkbd_stubs.c records the calls and lets the
 * test drive the "busy" state and the sent-character cursor.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>

typedef enum {
    CW_ENCODER_IDLE = 0,
    CW_ENCODER_SEND,
    CW_ENCODER_BEACON,
    CW_ENCODER_BEACON_IDLE,
} cw_encoder_state_t;

void cw_encoder_send(const char *text, bool beacon);
void cw_encoder_stop(void);

cw_encoder_state_t cw_encoder_state(void);
int                cw_encoder_sent(void);

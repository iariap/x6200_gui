/* Minimal events.h stub for the radio_thread test (issue #219): the keypad event
 * the Power key path enqueues. SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdint.h>

#include "lvgl/lvgl.h"

typedef enum {
    KEYPAD_UNKNOWN = 0,
    KEYPAD_POWER,
} keypad_key_t;

typedef enum {
    KEYPAD_PRESS = 0,
    KEYPAD_RELEASE,
    KEYPAD_LONG,
    KEYPAD_LONG_RELEASE,
} keypad_state_t;

typedef struct {
    keypad_key_t   key;
    keypad_state_t state;
} event_keypad_t;

extern uint32_t EVENT_KEYPAD;

void event_send(lv_obj_t *obj, lv_event_code_t event_code, void *param);

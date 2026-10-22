/*
 * events.h stub for the native spectrum test (issue #149): src/spectrum.c only
 * registers callbacks for the radio TX/RX events. The ids are the firmware's
 * (events.c assigns them with lv_event_register_id()); here they are plain
 * variables the test never sends.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

extern uint32_t EVENT_RADIO_TX;
extern uint32_t EVENT_RADIO_RX;

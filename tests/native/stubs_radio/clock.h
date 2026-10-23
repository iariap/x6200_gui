/* Minimal clock.h stub for the radio_thread test (issue #219): src/radio.c only
 * pushes the pack voltages into the power clock. SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stdint.h>

void clock_update_power(float ext, float bat, uint8_t cap, bool charge);

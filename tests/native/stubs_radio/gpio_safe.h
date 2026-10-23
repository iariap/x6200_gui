/* Minimal gpio_safe.h stub for the radio_thread test (issue #219): src/radio.c
 * only sets the Morse-key line. SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

void gpio_safe_set(int pin, int value);

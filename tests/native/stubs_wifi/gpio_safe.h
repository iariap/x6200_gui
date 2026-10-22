/*
 * Minimal gpio_safe.h for the WiFi idle-pump native stage (issue #218, F-06).
 * Only the one function src/wifi.cpp uses (no more): the stage records the writes to the
 * WiFi pin instead of touching a real GPIO.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void gpio_safe_set(int pin, int value);

#ifdef __cplusplus
}
#endif

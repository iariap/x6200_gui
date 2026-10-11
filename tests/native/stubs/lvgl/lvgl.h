/*
 * Minimal LVGL stub for the host/ARM unit tests.
 * Only the symbols used by adaptive_tuning.c and audio_enhancement.c.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Controllable clock (set from the tests with test_tick_set()). */
uint32_t lv_tick_get(void);

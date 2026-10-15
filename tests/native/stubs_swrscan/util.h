/*
 * Minimal util stub for the native SWR Scan dialog test (issue #85).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

void split_freq(int32_t freq, uint16_t *mhz, uint16_t *khz, uint16_t *hz);

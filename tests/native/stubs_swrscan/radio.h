/*
 * Minimal radio stub for the native SWR Scan dialog test (issue #85).
 *
 * The test stubs (tests/native/swrscan_stubs.c) record these calls and make
 * radio_start_swrscan() succeed, so the dialog reaches its "run" state.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

void     radio_set_freq(int32_t freq);
uint16_t radio_change_vol(int16_t df);
bool     radio_start_swrscan(void);
void     radio_stop_swrscan(void);

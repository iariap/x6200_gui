/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - C bridge for the C++ files that came from the X6100.
 *
 *  This GUI's headers have no extern "C" guards (its own code is C), so a C++
 *  caller cannot link against the C symbols directly. These two wrappers are
 *  what the ported remote server's settings shim needs.
 */

#include "band.h"
#include "../radio.h"

#include <stdint.h>

uint32_t x6200_bridge_band_read_all_bands(band_info_t **results, int32_t *cap) {
    return cfg_band_read_all_bands(results, cap);
}

bool x6200_bridge_radio_check_freq(int32_t freq) {
    return radio_check_freq(freq);
}

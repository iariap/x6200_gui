/*
 * theme.h stub for the native spectrum test (issue #149).
 *
 * src/spectrum.c asks for the colour of three roles; the stub gives each role a
 * distinct value so the test can tell the main trace from the peak trace in the
 * recorded lines (see tests/native/spectrum_stubs.h).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef enum {
    TC_text_dim = 0,
    TC_text_mute,
    TC_tx,
    TC_vfo_line,
} theme_color_t;

uint32_t theme_hex(theme_color_t c);

/*
 * Minimal theme.h for the native clock test (tests/native/test_clock.c).
 *
 * The clock never writes a colour: it asks the theme for a role, like the rest
 * of the ROM (src/theme.h: `TC(role)` for an lv_color_t). The stub keeps the
 * macro and the two roles src/clock.c uses so the test can check *which* role
 * each label is painted with; clock_stubs.c gives every role a distinct value.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "lvgl/lvgl.h"

typedef enum {
    TC_text = 0,
    TC_text_dim,
    TC_warn,
} theme_color_t;

uint32_t theme_hex(theme_color_t c);

#define TC(role) lv_color_hex(theme_hex(TC_##role))

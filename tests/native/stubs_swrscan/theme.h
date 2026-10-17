/*
 * Minimal theme stub for the native SWR Scan dialog test (issue #119).
 *
 * src/dialog_swrscan.c paints the chart's own background with the theme's panel
 * role (the same idiom as the app frames and the other dialogs' content tables),
 * so the test build needs the role and TC(). Only the roles the dialog uses are
 * declared; theme_hex() lives in tests/native/swrscan_stubs.c and returns a fixed
 * colour: the test checks THAT the chart paints its own background, not which
 * colour the theme gives it (the real palette is checked by the theme tests).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "lvgl/lvgl.h"

typedef enum {
    TC_panel,
} theme_color_t;

uint32_t theme_hex(theme_color_t c);

#define TC(role) lv_color_hex(theme_hex(TC_##role))

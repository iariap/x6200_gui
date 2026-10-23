/*
 * Minimal theme stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c uses theme_hex(TC_*) for the recolour strings it builds
 * and TC(role) for the state chip. theme_hex() here is theme-generation aware
 * (tests/native/cwkbd_stubs.c): bumping the generation changes every role's
 * colour, which is how the test proves a live theme change still repaints the
 * dialog (no stale colours).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "lvgl/lvgl.h"

/* Only the roles the CW keyboard dialog reads. */
typedef enum {
    TC_text,
    TC_text_dim,
    TC_text_head,
    TC_text_faint,
    TC_accent,
    TC_panel,
    TC_panel_border,
    TC_side,
    TC_chip,
    TC_warn,
    TC_warn_bg,
    TC_tx_bg,
    TC_COUNT,
} theme_color_t;

typedef struct {
    int16_t border;
} theme_shape_t;

extern theme_shape_t theme_shape;

uint32_t theme_hex(theme_color_t c);

#define TC(role) lv_color_hex(theme_hex(TC_##role))

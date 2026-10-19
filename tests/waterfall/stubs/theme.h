/* Stub de theme.h para el test determinista del waterfall (OPT-2, #148).
 * Solo lo que src/waterfall.c usa: el ancho de la linea central y el rol de
 * color del texto. SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdint.h>

#include "lvgl/lvgl.h"

typedef enum {
    TC_text = 0,
} theme_color_t;

typedef struct {
    int16_t waterfall_signal_width;
} theme_shape_t;

extern theme_shape_t theme_shape;

uint32_t theme_hex(theme_color_t role);

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - country flags for the FT8 rows.
 *
 *  flags_data.h is the X6100's generated header (tools/flags_gen.py: 16x12 flags
 *  from flagcdn.com, public domain, indexed by the primary prefix of AD1C's cty.dat,
 *  ISO codes from k0swe/dxcc-json under MIT). It holds RGB888 pixels; LVGL wants
 *  RGB565, so this module converts each flag the first time it is asked for and
 *  keeps the descriptor.
 */

#pragma once

#include "lvgl/lvgl.h"

#include <stdbool.h>
#include <stdint.h>

/* RGB888 (0xRRGGBB) to the RGB565 LVGL uses. Pure, so it is unit tested. */
uint16_t flags_rgb888_to_565(uint32_t rgb);

/* The image of a flag index, or NULL when there is none */
const lv_img_dsc_t *flags_image(int index);

/* Total number of flags, and the prefix the generator assigned each one */
int  flags_count(void);
bool flags_index_of_prefix(const char *prefix, int *index);

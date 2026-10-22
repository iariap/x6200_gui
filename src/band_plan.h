/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - IARU Region 1 band plan strip
 *
 *  A thin coloured strip over the bottom of the spectrum (lined up with the
 *  frequency row) showing what each part of the displayed span is for: CW,
 *  digital modes, all modes/SSB and beacons.
 *
 *  Ported from the X6100 firmware by IU1WXY (src/band_plan.c); here the theme
 *  colours became this GUI's fixed palette and the segment maths moved to
 *  band_plan_data.c so it can be unit tested.
 */

#pragma once

#include "band_plan_data.h"

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BAND_PLAN_H     22      /* strip height */

/* Strip on `parent` at y; call after the waterfall is created so it stays on top */
void band_plan_init(lv_obj_t *parent, lv_coord_t y);

/* Move the strip (when the layout above it changes) */
void band_plan_set_y(lv_coord_t y);

/* Span shown: center - half_width .. center + half_width */
void band_plan_update(int32_t center, int32_t half_width);

/* The setting changed: redraw with the last span */
void band_plan_refresh(void);

#ifdef __cplusplus
}
#endif

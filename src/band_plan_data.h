/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - IARU Region 1 band plan data and geometry
 *
 *  Pure C (no LVGL): the plan table and the maths that maps it to the strip, so
 *  it can be unit tested on the host. The drawing lives in band_plan.c.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAND_PLAN_W     800     /* strip width in pixels (full screen width) */
#define BAND_PLAN_MAX   24      /* segments that can be shown at once */

typedef enum {
    BP_CW = 0,
    BP_DIG,
    BP_ALL,
    BP_BCN,
} bp_type_t;

typedef struct {
    int32_t   x;        /* left edge, in strip pixels */
    int32_t   w;        /* width, in strip pixels */
    bp_type_t type;
} bp_visible_t;

/* Band plan segments visible in [center - half_width, center + half_width].
 * Returns how many segments were written, never more than max_out. */
int band_plan_segments(int32_t center, int32_t half_width, bp_visible_t *out, int max_out);

#ifdef __cplusplus
}
#endif

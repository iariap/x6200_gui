/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - GPS position (see gps.h).
 */

#include "gps.h"

#include <stddef.h>

static bool no_fix(double *lat, double *lon, int *age_s) {
    if (lat) { *lat = 0.0; }
    if (lon) { *lon = 0.0; }
    if (age_s) { *age_s = 0; }

    return false;
}

bool gps_last_fix(double *lat, double *lon, int *age_s) {
    return no_fix(lat, lon, age_s);
}

bool gps_phone_fix(double *lat, double *lon, int *age_s) {
    return no_fix(lat, lon, age_s);
}

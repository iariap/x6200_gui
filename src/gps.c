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

static bool    phone_have;
static double  phone_lat, phone_lon;

bool gps_phone_fix(double *lat, double *lon, int *age_s) {
    if (!phone_have) {
        return no_fix(lat, lon, age_s);
    }

    if (lat) { *lat = phone_lat; }
    if (lon) { *lon = phone_lon; }
    if (age_s) { *age_s = 0; }

    return true;
}

void gps_phone_set(double lat, double lon) {
    phone_lat  = lat;
    phone_lon  = lon;
    phone_have = true;
}

void gps_phone_clear(void) {
    phone_have = false;
}

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - GPS position (not available in this build).
 *
 *  The X6200 firmware this ROM is based on has no GPS stack (gpsd was removed),
 *  so the station locator always falls back to the saved QTH. The ported app
 *  code asks for a fix; these return "no fix".
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool gps_last_fix(double *lat, double *lon, int *age_s);
bool gps_phone_fix(double *lat, double *lon, int *age_s);

#ifdef __cplusplus
}
#endif

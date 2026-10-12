/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  JS8 map: the world with a path from the station to each QSO of the log
 */

#pragma once

#include "lvgl/lvgl.h"

/* A w x h map on parent, centred on the station's longitude: coastlines, the station
 * (my_grid) and a great-circle path to each call of the ADIF log at log_path */
lv_obj_t *js8_map_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const char *my_grid, const char *log_path);

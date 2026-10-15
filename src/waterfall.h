/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <unistd.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

lv_obj_t * waterfall_init(lv_obj_t * parent);
void waterfall_data(float *data_buf, uint16_t size, bool tx);
void waterfall_set_height(lv_coord_t h);
void waterfall_min_max_reset();

void waterfall_update_max(float db);
void waterfall_update_min(float db);
void waterfall_refresh_reset();
void waterfall_refresh_period_set(uint8_t k);
void waterfall_set_battery_mode(bool enabled);

/* Redibujos offscreen de la paleta que efectivamente corrieron (instrumento del
 * issue #13): no cuenta los que se saltearon con la pantalla apagada
 * (backlight_switch() -> lv_disp_enable_invalidation(false)). Lo mide el arnes
 * con `--bench-natural --display-off`. */
uint32_t waterfall_offscreen_redraw_count(void);

/* The levels the waterfall is drawing with, for the remote scope */
void waterfall_get_levels(bool tx, float *min, float *max);

/* Vista actual del waterfall (centro y mitad del ancho) */
void waterfall_view(int32_t *center, int32_t *half);

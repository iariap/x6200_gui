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

lv_obj_t * spectrum_init(lv_obj_t * parent);
void spectrum_data(float *data_buf, uint16_t size, bool tx);
void spectrum_min_max_reset();

/* Cadencia del redibujo del espectro: dibuja 1 de cada `frames` refrescos (el
 * mismo mecanismo del modo Ahorro del waterfall, src/waterfall.c). Lo fija el
 * perfil visual (visual_policy_spectrum_apply); `frames` 0 se ignora y 1 vuelve a
 * dibujar siempre. */
void spectrum_set_refresh_period(uint8_t frames);

float spectrum_get_min();
void spectrum_update_max(float db);
void spectrum_update_min(float db);
void spectrum_clear();
// void spectrum_update_filters();
// void spectrum_update_factor();

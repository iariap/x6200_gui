/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2024 Georgy Dyuldin aka R2RFE
 */

#pragma once
#include <stdbool.h>
#include <lvgl/lvgl.h>

void cw_tune_init(lv_obj_t * parent);

// bool cw_tune_toggle(int16_t diff);

void cw_tune_set_freq(float hz);

/* Si el widget esta en pantalla y visible: es lo que decide el feed del tono (src/cw.cpp), porque
 * el bloque activo que marca donde esta el tono lo alimenta ese feed. */
bool cw_tune_ui_visible(void);

/* The X6100 remembers the last CW tuning tone and the remote server offers it.
 * This GUI does not keep it, so the answer is always "nothing recent". */
bool cw_tune_recent(int32_t *hz);

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

/* Instrumento OPT-2 (#148): pixeles mapeados (division + lookup de paleta) por
 * el rasterizado del waterfall desde el arranque. Con el anillo de filas es
 * WIDTH por fila nueva en reposo, contra WIDTH*height por fila del baseline. */
uint64_t waterfall_rasterized_px(void);

/* Reconstrucciones completas de la paleta (mapeo cambiado o historial
 * incompatible), para el informe A/B de #148. */
uint32_t waterfall_full_rebuild_count(void);

/* Indice de la fila mas nueva dentro del frame del anillo (la ventana visible
 * empieza ahi); los fixtures de #148 aserten el offset del anillo. */
uint16_t waterfall_render_head(void);

/* La imagen rasterizada del anillo (WIDTH x height px, TRUE_COLOR 32 bits) y su
 * alto, para que los fixtures OPT-2 (#148) comparen el framebuffer interno byte
 * a byte contra el baseline. NULL si todavia no se armo. */
const lv_color_t *waterfall_frame_pixels(void);
lv_coord_t waterfall_frame_height(void);

/* Linea central del waterfall (issue #140): el objeto vivo, para que el arnes de
 * UI codifique su geometria (largo = alto del waterfall, centrada, oculta con el
 * parametro apagado). NULL si todavia no se creo. */
lv_obj_t * waterfall_middle_line(void);

/* Ticks del ancho del filtro RX (issue #173, PR 1): el objeto vivo i (0=izq,
 * 1=der) sobre el borde superior de la cascada, para que el arnes de UI
 * codifique su geometria. NULL si no existe o `i` esta fuera de rango. */
lv_obj_t * waterfall_filter_tick(int i);

/* The levels the waterfall is drawing with, for the remote scope */
void waterfall_get_levels(bool tx, float *min, float *max);

/* Vista actual del waterfall (centro y mitad del ancho) */
void waterfall_view(int32_t *center, int32_t *half);

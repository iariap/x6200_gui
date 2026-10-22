/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - marcas de los spots del cluster sobre el waterfall.
 *
 *  Portado del X6100: los spots que pasan los filtros del cluster se dibujan como una etiqueta
 *  con el indicativo y una linea hasta su frecuencia, en dos filas escalonadas. Los colores
 *  salen del tema (spot = TC(ok), alerta = TC(err)), como el "cyan ... an alert call stays pink"
 *  del original. Sin parametro que lo active: si el cluster no tiene spots, no se dibuja nada.
 */

#pragma once

#include "lvgl/lvgl.h"

#include <stdbool.h>
#include <stdint.h>

/* Alto de la franja: dos filas de 22 */
#define SPOT_MARKS_H 44

void spot_marks_init(lv_obj_t *parent, lv_coord_t line_y, lv_coord_t tag_y);
void spot_marks_layout(lv_coord_t line_y, lv_coord_t tag_y);

/* La vista que hay que dibujar (centro y mitad del ancho visible): la llama el waterfall */
void spot_marks_update(int32_t center, int32_t half_width);

/* El primer spot que queda entre dos frecuencias al sintonizar (para el enganche) */
bool spot_marks_crossed(int32_t from, int32_t to, int32_t *spot_hz);

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Design tokens de layout (issue #10, UI-05 incremento 1).
 *
 *  Nombres de dominio para la geometria que hasta ahora eran literales sueltos
 *  en src/info.c (la barra de info de seis celdas, el nido de numeros magicos
 *  donde vivieron UI-011/UI-012). Es un refactor puro: cada token vale
 *  exactamente el literal al que reemplaza, asi que el render no cambia un
 *  pixel (lo verifica el gate A/B del arnes: `changed_pixels 0`).
 *
 *  Alcance de este incremento: solo la barra de info. El resto de las regiones
 *  (botones, band_plan, filas de Ajustes) sigue con sus literales hasta sus
 *  propios incrementos, y spectrum/waterfall/band_plan quedan fuera a proposito
 *  (son areas de dibujo fijo; el issue prohibe convertirlas a ciegas).
 *
 *  Este header es la capa de constantes. Consumirlas desde el JSON de tema es
 *  el incremento siguiente, por eso aca no hay estado del tema ni logica: solo
 *  los numeros, mas la fuente de la celda (que el fix UI-012 necesita nombrar).
 */

#pragma once

#include "styles.h"         /* sony_20: la fuente por defecto de la celda */

/* Rejilla de la barra: 3 columnas x 2 filas (el orden de info_items_t en
 * src/info.c, y el tamano del array de items). */
#define UI_INFO_COLS            3
#define UI_INFO_ROWS            2

/* Celda: la barra se apoya en un bitmap de fondo (images/top_short*.bin) y las
 * seis celdas se pintan encima; el tamano y el pitch son los del bitmap, no una
 * decision de layout libre. */
#define UI_INFO_CELL_W          56
#define UI_INFO_CELL_H          26
#define UI_INFO_CELL_GAP        2       /* separacion horizontal entre celdas */
#define UI_INFO_COL_PITCH       (UI_INFO_CELL_W + UI_INFO_CELL_GAP)     /* 58 */
#define UI_INFO_ROW_PITCH       22      /* los renglones se solapan 4 px (26 > 22) */

/* Origen de la rejilla dentro del objeto de la barra. */
#define UI_INFO_ORIGIN_X        15
#define UI_INFO_ORIGIN_Y        5

/* Radio de la celda que pinta cada item (TC(bar) sobre el bitmap). */
#define UI_INFO_CELL_RADIUS     3

/* Celda 6 (WiFi/BT): la comparten los dos. El WiFi ocupa la mitad izquierda
 * --el ancho de celda partido al medio-- y la runa de BT la derecha, porque el
 * S-meter empieza justo al lado de la barra y no entra una columna mas. */
#define UI_INFO_WIFI_COL        2
#define UI_INFO_WIFI_ROW        1
#define UI_INFO_WIFI_W          (UI_INFO_CELL_W / 2)    /* 28 */
#define UI_INFO_BT_LINE_W       2
#define UI_INFO_BT_POINTS       7       /* puntos de la runa: debe coincidir con bt_pts[] */
#define UI_INFO_BT_DY           8       /* la runa baja 8 px dentro de la celda */

/* Fuente por defecto de la celda (la de info_item_style, src/styles.c). Bajo
 * VISION la celda del VFO baja un tamano desde aca (fix UI-012): el token nombra
 * la fuente que el fix EXP-2 tenia cableada. */
#define UI_INFO_FONT            (&sony_20)

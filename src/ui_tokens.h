/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Design tokens de layout (issue #10, UI-05).
 *
 *  Nombres de dominio para la geometria que hasta ahora eran literales sueltos
 *  en src/info.c (la barra de info, el nido de numeros magicos donde vivieron
 *  UI-011/UI-012; incremento 1) y en src/main_screen.c (la fila de frecuencia
 *  del dial, los labels `freq[]`; incremento 3). Es un refactor puro: cada
 *  token vale exactamente el literal al que reemplaza, asi que el render no
 *  cambia un pixel (lo verifica el gate A/B del arnes: `changed_pixels 0`).
 *
 *  Fuera por ahora: el resto de las regiones (botones, band_plan, filas de
 *  Ajustes) sigue con sus literales hasta sus propios incrementos, y
 *  spectrum/waterfall/band_plan quedan fuera a proposito (son areas de dibujo
 *  fijo; el issue prohibe convertirlas a ciegas).
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
 * decision de layout libre. El aire horizontal entre celdas y el pitch vertical
 * los fija el issue #251: 6 px de separacion (antes 2) y 26 px de renglon (antes
 * 22, que solapaba las dos filas 4 px). */
#define UI_INFO_CELL_W          56
#define UI_INFO_CELL_H          26
#define UI_INFO_CELL_GAP        6       /* separacion horizontal entre celdas (#251) */
#define UI_INFO_COL_PITCH       (UI_INFO_CELL_W + UI_INFO_CELL_GAP)     /* 62 */
#define UI_INFO_ROW_PITCH       26      /* los renglones ya no se solapan (26 == CELL_H) */

/* Padding vertical local de la celda (#251): el texto de estado de 22 px tiene que
 * entrar en las 26 px de la celda (26 - 2*2 = 22). Se aplica sobre el objeto (en
 * src/info.c), no en info_item_style, que comparten las seis celdas. */
#define UI_INFO_CELL_PAD_V      2

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

/* La runa de BT (issue #285) son DOS trazos persistentes (src/info.c): los
 * brazos en zigzag (`bt_arm_pts[]`, path A) y la ESPINA vertical
 * (`bt_spine_pts[]`, path B). Su ancho de trazo, su cuenta de puntos y su caja
 * local salen de aca; la x la fija UI_INFO_BT_X_PAD (el aire a la izquierda
 * dentro de la media celda derecha, 28 px, centro x181) y la y la ecuacion de
 * la fila. Caja local de los brazos: x3..15, y1..21. */
#define UI_INFO_BT_LINE_W       2       /* grosor del trazo de las dos lineas */
#define UI_INFO_BT_ARM_POINTS   7       /* puntos de los brazos: debe coincidir con bt_arm_pts[] */
#define UI_INFO_BT_SPINE_POINTS 2       /* puntos de la espina: debe coincidir con bt_spine_pts[] */
#define UI_INFO_BT_W            15      /* max x de los brazos (bt_arm_pts[]) */
#define UI_INFO_BT_H            21      /* max y de los brazos (bt_arm_pts[]) */
#define UI_INFO_BT_X_PAD        6       /* aire a la izquierda dentro de la media celda derecha */
#define UI_INFO_BT_DY           8       /* la runa baja 8 px dentro de la celda */

/* Fuente por defecto de la celda (la de info_item_style, src/styles.c). Bajo
 * VISION la celda del VFO baja un tamano desde aca (fix UI-012): el token nombra
 * la fuente que el fix EXP-2 tenia cableada. */
#define UI_INFO_FONT            (&sony_20)

/* La fuente de los tres estados compactos (AGC, PRE/ATT, ATU, #251): el slot
 * "grande" de la celda, el mas ancho que todavia entra en los 56 px. VFO, modo y
 * WiFi NO la usan: sus textos son mas largos -- medidos en el arnes `USB-D` 49 px,
 * `VFO-A` 48, `SPL-A` 46, `CH 19` 41 -- y no tienen ese aire.
 *
 * Nota medida: la migracion de fuentes de #9 genera las familias de los seis temas
 * con los slots 20 y 22 al MISMO tamano de pixel, asi que en la coleccion este
 * cambio de slot no agranda el glifo; con la familia stock del equipo (14 px vs
 * 15 px) si. Lo que #251 cambia siempre es la caja del texto (UI_INFO_CELL_PAD_V)
 * y el aire de la rejilla. Medido en docs/evidencia/2026-10-23-home251/. */
#define UI_INFO_FONT_BIG        (&sony_22)

/* ------------------------------------------------------------------------ *
 * La rejilla tiene que entrar en el fondo de la barra (`info_style`, 206 px en
 * los bitmaps legacy y 209 en los simples, siempre 61 de alto, src/styles.c) y
 * los dos renglones NO pueden pisarse: era el defecto del issue #251 (pitch 22
 * con celdas de 26 = 4 px de solape). Se verifica en compilacion, sin depender del
 * tema (la barra mas angosta es la legacy de 206 y el alto es 61 en las dos).
 * Los numeros 206/61 se repiten aca a proposito: son la caja del estilo, que no
 * tiene token propio (`info_style`, src/styles.c). */
_Static_assert(UI_INFO_ROW_PITCH >= UI_INFO_CELL_H,
               "info: los dos renglones de la barra no pueden solaparse");
_Static_assert(UI_INFO_ORIGIN_X + UI_INFO_COLS * UI_INFO_CELL_W
                   + (UI_INFO_COLS - 1) * UI_INFO_CELL_GAP <= 206,
               "info: la ultima celda se sale del fondo de la barra (206 px)");
_Static_assert(UI_INFO_ORIGIN_Y + (UI_INFO_ROWS - 1) * UI_INFO_ROW_PITCH + UI_INFO_CELL_H <= 61,
               "info: la ultima fila se sale del fondo de la barra (61 px)");

/* La runa de BT (dos trazos, src/info.c) tiene que entrar en la media celda
 * derecha de la celda WiFi/BT: caja x167..195 y y31..61, con el trazo de 2 px
 * YA CONTADO -- LVGL suma el line width al tamano del objeto
 * (`lv_line_get_self_size`, lvgl/src/widgets/lv_line.c), asi que la caja real
 * es la de los puntos mas UI_INFO_BT_LINE_W. La x sale de la formula de la celda
 * (la media celda derecha empieza en ORIGIN_X + WIFI_COL*COL_PITCH + WIFI_W, 167
 * -- el mismo termino que la celda de al lado) y la y de la ecuacion de la fila:
 * los dos trazos comparten origen, asi que se mueven como una sola unidad.
 * Los 167/195/31/61 son la media celda y la banda que el contrato fija, y se
 * repiten aca a proposito. */
#define UI_INFO_BT_X0   (UI_INFO_ORIGIN_X + UI_INFO_WIFI_COL * UI_INFO_COL_PITCH \
                         + UI_INFO_WIFI_W + UI_INFO_BT_X_PAD)
#define UI_INFO_BT_Y0   (UI_INFO_WIFI_ROW * UI_INFO_ROW_PITCH + UI_INFO_BT_DY)

_Static_assert(UI_INFO_BT_X0 >= 167,
               "info: la runa de BT arranca antes de la media celda derecha (x167)");
_Static_assert(UI_INFO_BT_X0 + UI_INFO_BT_W + UI_INFO_BT_LINE_W <= 195,
               "info: la runa de BT se sale de la media celda derecha (x195)");
_Static_assert(UI_INFO_BT_Y0 >= 31,
               "info: la runa de BT arranca arriba de la banda de la celda (y31)");
_Static_assert(UI_INFO_BT_Y0 + UI_INFO_BT_H + UI_INFO_BT_LINE_W <= 61,
               "info: la runa de BT se sale de la barra (y61) con el trazo de 2 px");

/* ------------------------------------------------------------------------ *
 * Fila de frecuencia del dial (src/main_screen.c): los tres labels `freq[]`.
 *
 * El extremo izquierdo y el derecho (150 px cada uno) son los numeros chicos
 * de los bordes del dial; el central (500 px) es la frecuencia grande. Teselan
 * el ancho fijo del panel (800 px, el 800x480 que fija el issue):
 * 150 + 500 + 150 = 800, por eso el label central arranca en 150 y el derecho
 * en 650. Las anchuras no son una decision de layout libre: son las de
 * `freq_style` / `freq_main_style` (src/styles.c, fuera de este incremento), y
 * las x salen de esas anchuras -- por eso el token nombra la relacion en vez de
 * repetir el numero. No hay un pitch de columna: los tres anchos son distintos.
 *
 * La fila se apila debajo del espectro con el cursor `y` de main_screen.c
 * (`y += spectrum_height` con `spectrum_height = 480 / 3`): su Y no es un
 * literal sino el cursor del layout apilado, y el espectro queda fuera del
 * issue, asi que este incremento no lo tokeniza. */

#define UI_FREQ_SCREEN_W        800     /* ancho fijo del panel */
#define UI_FREQ_SIDE_W          150     /* ancho de cada extremo (freq_style) */
#define UI_FREQ_MAIN_W          500     /* ancho del label central (freq_main_style) */
#define UI_FREQ_ROW_H           36      /* alto de la fila (freq_style/freq_main_style) */

/* Origen x de cada columna, tal como lo calcula la fuente. */
#define UI_FREQ_LEFT_X          0
#define UI_FREQ_CENTER_X        (UI_FREQ_SCREEN_W / 2 - UI_FREQ_MAIN_W / 2)     /* 150 */
#define UI_FREQ_RIGHT_X         (UI_FREQ_SCREEN_W - UI_FREQ_SIDE_W)             /* 650 */

/* La fila es una tesela: los dos extremos y el centro cubren el panel entero,
 * sin hueco ni solape. Si un ancho cambia sin el otro, la fila deja de cubrir
 * el ancho de la pantalla; el contrato se verifica en compilacion. */
_Static_assert(2 * UI_FREQ_SIDE_W + UI_FREQ_MAIN_W == UI_FREQ_SCREEN_W,
               "fila de frecuencia: extremos + centro deben teselar el panel");

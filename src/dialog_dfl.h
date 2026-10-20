/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Editor DFL (issue #173, PR 2): dialogo modal compacto sobre el waterfall con
 *  el esquema de banda pasante y los controles LOW / HIGH / BW.
 *
 *  Es un dialogo del framework (src/dialog.h), no una app: no guarda ni
 *  restaura el estado de la radio (restore_radio = false). Su modelo puro
 *  (pasos, limites, geometria, BW y reset) vive en src/dfl_model.h.
 */

#pragma once

#include "dialog.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

extern dialog_t *dialog_dfl;

/* Etiquetas que el arnes usa para ubicar los controles del dialogo por su
 * user_data (mismo mecanismo que src/buttons.cpp) y empujar una pulsacion por
 * el callback real. No son API de la radio. */
typedef enum {
    DFL_UI_GRAPH = 1,
    DFL_UI_LOW_MINUS,
    DFL_UI_LOW_PLUS,
    DFL_UI_HIGH_MINUS,
    DFL_UI_HIGH_PLUS,
    DFL_UI_BW_MINUS,
    DFL_UI_BW_PLUS,
    DFL_UI_RESET,
    DFL_UI_CLOSE,
} dfl_ui_id_t;

/* --- Seam de test (host) ----------------------------------------------------
 * La radio lee el puntero del indev activo de LVGL para el arrastre de los
 * handles. El host no tiene touch: el arnes instala aca su lector de un punto
 * sintetico. En la radio queda NULL y el arrastre usa el indev de LVGL. */
extern bool (*dialog_dfl_pointer_hook)(lv_point_t *out);

#ifdef __cplusplus
}
#endif

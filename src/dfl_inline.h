/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Editor DFL INLINE (issue #179, correccion del #173).
 *
 *  Sustituye al dialogo modal de #176/#177 por un panel NATIVO integrado en el
 *  rectangulo del waterfall. No es un dialogo: no usa dialog_construct(),
 *  dialog_destruct(), ni X, titulo, marco, sombra o fondo de ventana; no pide
 *  main_screen_keys_enable(false). Se dibuja sobre el waterfall dejando vivos
 *  sus 28 px SUPERIORES (las filas mas recientes de la cascada) y muestra, de
 *  izquierda a derecha, el grafico protagonista de banda pasante con handles y
 *  las tres filas LOW/HIGH/BW con su valor exacto en Hz.
 *
 *  El DFL se edita SOLO con el encoder de VOLUMEN (rotary1, arriba a la
 *  izquierda), cuyos eventos llegan como KEY_VOL_*_EDIT / KEY_VOL_*_SELECT, y
 *  con touch en las filas/handles. La rueda principal VFO (rotary0) NUNCA toca
 *  el filtro: sigue sintonizando.
 *
 *  La decision y la aplicacion por los setters vigentes viven en
 *  src/dfl_model.h (puro) y src/dfl_rotary.c; aca vive solo la UI inline.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

#include "dfl_model.h"

/* Etiquetas que el arnes usa para ubicar los controles por su user_data (mismo
 * mecanismo que src/buttons.cpp). No son API de la radio. */
typedef enum {
    DFL_UI_GRAPH = 1,
    DFL_UI_LOW_ROW,
    DFL_UI_HIGH_ROW,
    DFL_UI_BW_ROW,
    DFL_UI_RESET,
} dfl_inline_ui_id_t;

/* Arma el panel inline como hijo del objeto del waterfall (`waterfall_widget`),
 * derivando y/h del objeto y de la barra de botones (no hardcodea 240). Deja el
 * panel oculto. Idempotente. */
void dfl_inline_init(lv_obj_t *waterfall_widget);

/* Suelta observers y punteros. El objeto LVGL se borra con la pantalla. */
void dfl_inline_shutdown(void);

/* Muestra/oculta el panel y cambia la barra inferior a su pagina expandida /
 * normal. Guardan y restauran el estado del encoder VOL. */
void dfl_inline_expand(void);
void dfl_inline_collapse(void);
void dfl_inline_toggle(void);
bool dfl_inline_is_expanded(void);

/* Parametro seleccionado para el encoder VOL. */
dfl_sel_t dfl_inline_get_sel(void);
void      dfl_inline_select(dfl_sel_t sel);

/* Ruteo del encoder VOL (rotary1). Devuelve true SOLO si el panel esta
 * expandido y el evento se consumio (no debe llegar a vol_update()). */
bool dfl_inline_on_vol_key(uint32_t key);

/* TX/RX: atenua/deshabilita la edicion RX durante TX. */
void dfl_inline_on_radio_event(lv_event_code_t code);

/* --- Seam de test (host) ----------------------------------------------------
 * La radio lee el puntero del indev activo de LVGL para el arrastre de los
 * handles. El host no tiene touch: el arnes instala aca su lector de un punto
 * sintetico. En la radio queda NULL y el arrastre usa el indev de LVGL. */
extern bool (*dfl_inline_pointer_hook)(lv_point_t *out);

#ifdef __cplusplus
}
#endif

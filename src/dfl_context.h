/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Banda contextual del DFL (issue #183).
 *
 *  Un rotulo chico en el area superior del waterfall para los textos de las
 *  paginas DFL que no tienen celda propia: el aviso `Personalizado` de la pagina
 *  de Favoritos, el contexto `Guardar Mem n · LOW ... · HIGH ...` de la
 *  confirmacion y el `Asignar tecla n` + la accion TACTIL `Cancelar` de la
 *  asignacion. No es un popup, ni una ventana, ni un marco, ni una sexta tecla:
 *  es un chip discreto que no tapa el waterfall completo ni el S-meter.
 *
 *  Sin SQL, sin estado de negocio: la UI (src/buttons.cpp) decide cuando
 *  mostrarlo y que callback dispara el chip.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

/* Etiquetas que el arnes usa para ubicar los controles por su user_data (mismo
 * mecanismo que src/buttons.cpp y src/dfl_inline.c). No son API de la radio.
 * Los ids viven en un espacio propio (0x81xx): el arnes busca por user_data en
 * todo el arbol y los ids de dfl_inline.c (1..5) no se pueden repetir aca. */
typedef enum {
    DFL_CTX_LABEL = 0x8101,
    DFL_CTX_ACTION = 0x8102,
} dfl_context_ui_id_t;

/* Crea la banda como hermana del waterfall (hijo de la pantalla), alineada al
 * area superior derecha del waterfall real. Queda oculta. Idempotente. */
void dfl_context_init(lv_obj_t *waterfall_widget);

/* Suelta punteros. El objeto LVGL se borra con la pantalla. */
void dfl_context_shutdown(void);

/* Muestra solo texto. */
void dfl_context_show_text(const char *text);

/* Muestra texto y una accion tactil (chip a la derecha) que dispara `cb`. */
void dfl_context_show_action(const char *text, const char *action, void (*cb)(void));

void dfl_context_hide(void);
bool dfl_context_visible(void);

#ifdef __cplusplus
}
#endif

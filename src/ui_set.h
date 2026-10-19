/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Guardas de igualdad para los updaters de la GUI (OPT-4, issue #150).
 *
 *  lv_label_set_text y lv_obj_set_style_* invalidan el objeto aunque el valor
 *  sea el mismo (LVGL 8.3.11 no compara), y varios updaters corren por timer o
 *  desde mas de un observer, asi que reescriben valores identicos y despiertan
 *  refrescos que no cambian un pixel. Estas guardas leen el valor VIVO del
 *  objeto (lv_label_get_text / lv_obj_get_style_*), nunca un cache: el valor
 *  resuelto del objeto ya refleja el tema/fuente vigente, asi que un cambio de
 *  tema o de fuente sigue aplicandose.
 *
 *  No son un cambio de presentacion: si el valor no cambio, no habia pixel que
 *  actualizar; el framebuffer queda byte-identico (lo verifica el arnes).
 */

#pragma once

#include <string.h>

#include "lvgl/lvgl.h"

/* Setea el texto solo si cambio. Devuelve true si escribio (e invalido). */
static inline bool ui_label_set_text(lv_obj_t *obj, const char *text) {
    if (strcmp(lv_label_get_text(obj), text) == 0) {
        return false;
    }
    lv_label_set_text(obj, text);
    return true;
}

/* Setea el color de texto solo si cambio. */
static inline bool ui_obj_set_text_color(lv_obj_t *obj, lv_color_t color) {
    if (lv_obj_get_style_text_color(obj, 0).full == color.full) {
        return false;
    }
    lv_obj_set_style_text_color(obj, color, 0);
    return true;
}

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - topes y pasos de los controles de las apps.
 *
 *  Lo que en el X6100 hace controls.cpp con update_param(): se aplica el giro del MFK y se
 *  recorta al rango que tenía el control en el X6100. Está aparte del registro (que toca la
 *  configuración) para poder probarlo en el host sin arrastrar la GUI.
 */

#include "controls_ids.h"

#include <stdint.h>

/* Rangos del X6100 (settings_manager.h): -1300..1300 y -400..400 */
#define WEFAX_ALIGN_MAX  1300
#define WEFAX_ALIGN_MIN  (-1300)
#define WEFAX_TILT_MAX   400
#define WEFAX_TILT_MIN   (-400)

static int32_t limita(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

/* Devuelve el valor nuevo, ya recortado. Si el control no es de este port devuelve `actual`. */
int32_t controls_step(int ctrl, int32_t actual, int32_t diff) {
    switch (ctrl) {
        case CTRL_WEFAX_ALIGN:
            return limita(actual + diff, WEFAX_ALIGN_MIN, WEFAX_ALIGN_MAX);

        case CTRL_WEFAX_TILT:
            return limita(actual + diff, WEFAX_TILT_MIN, WEFAX_TILT_MAX);

        default:
            return actual;
    }
}

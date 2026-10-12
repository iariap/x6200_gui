/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - registro de controles de las apps (lo que el X6100 hace en
 *  controls.cpp). Cuando una app toma un control (mfk_set_ctrl) los giros del MFK tienen que
 *  mover ESE control y no el parámetro del MFK de la pantalla principal.
 *
 *  Está en C a propósito: así los tests nativos pueden cubrir los topes y los pasos sin
 *  arrastrar LVGL ni el resto de la GUI.
 */

#include "controls_ids.h"
#include "controls_step.h"
#include "cfg/cfg.h"
#include "cfg/cfg_api.h"
#include "params/params.h"

#include "lvgl/lvgl.h"


/* Aplica `diff` al control y devuelve el valor nuevo (0 si el control no es de este port). */
int32_t controls_encoder_update(int ctrl, int32_t diff) {
    int32_t v;

    switch (ctrl) {
        case CTRL_WEFAX_ALIGN:
            v = controls_step(CTRL_WEFAX_ALIGN, param_i_get(cfg_wefax_align), diff);
            param_i_set(cfg_wefax_align, v);
            LV_LOG_USER("WeFax align: %i", v);
            return v;

        case CTRL_WEFAX_TILT:
            v = controls_step(CTRL_WEFAX_TILT, param_i_get(cfg_wefax_tilt), diff);
            param_i_set(cfg_wefax_tilt, v);
            LV_LOG_USER("WeFax tilt: %i", v);
            return v;

        default:
            return 0;
    }
}

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Capa de aplicacion del editor DFL (issue #173, PR 2 / correccion del owner).
 *
 *  El dialogo (src/dialog_dfl.c) no escribe los Subjects del filtro: pide aca.
 *  Todo pasa por los caminos vigentes del firmware:
 *
 *    - LOW  -> cfg_mode_set_low_filter()   (valida: high > low, LOW >= 0 en SSB,
 *                                          LOW negativo en CW/CWR, no-op en
 *                                          AM/SAM/NFM/WFM).
 *    - HIGH -> cfg_mode_set_high_filter()  (valida: high > low, high <= MAX).
 *    - BW   -> subject_set_int(cfg_cur.filter.bw), disparando el observer de
 *              src/cfg/mode.c::on_cur_filter_bw_change(); el par LOW/HIGH
 *              resultante se valida ANTES de escribir (dfl_bw_legal), igual que
 *              el resto del editor.
 *
 *  Ninguna funcion devuelve un valor optimista: SIEMPRE relee el Subject
 *  despues de aplicar, asi que un rechazo del setter deja el valor confirmado.
 *  El modulo no usa LVGL (solo Subjects, setters y msg.h), por eso el test
 *  nativo lo compila contra stubs de src/cfg/mode.c y de los Subjects.
 */

#pragma once

#include "dfl_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Valor que quedo en el Subject tras aplicar `val`. Nunca el pedido. */
int32_t dfl_set_low(int32_t val);
int32_t dfl_set_high(int32_t val);
int32_t dfl_set_bw(int32_t val);

/* Un paso de la rueda sobre el parametro seleccionado, con los pasos de
 * src/vol.c (dfl_sel_step). Emite un mensaje corto con el valor APLICADO y
 * devuelve ese mismo valor (el del Subject). Si el parametro no es editable en
 * el modo (LOW en AM/SAM/NFM/WFM) o el resultado no es legal (BW), no escribe
 * nada y el mensaje muestra el valor confirmado. */
int32_t dfl_rotary_apply(dfl_sel_t sel, int32_t diff);

#ifdef __cplusplus
}
#endif

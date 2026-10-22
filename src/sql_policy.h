/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Politica del squelch de voz (issue #166).
 *
 *  La UI (src/vol.c) y el remoto (src/remote_server.cpp) mueven el squelch en
 *  la escala 0..100. La lib del control (aether-x6200-control) documenta
 *  `x6200_control_sql_set()` como "Voice SQL 0 - 100" y expone un enable
 *  separado (`x6200_control_sql_enable_set()`). La convencion de esta GUI es:
 *
 *      nivel == 0  ->  squelch ABIERTO   (enable off)
 *      nivel  > 0  ->  squelch HABILITADO (enable on, el nivel gobierna)
 *
 *  Es la misma convencion que ya usa la lib internamente (sql_set/sql_fm_set
 *  llaman a enable con `sql > 0`), pero se aplica de forma explicita desde el
 *  camino de radio para que el contrato no dependa de esa version de la lib y
 *  quede cubierto tambien el arranque. Ver docs/HALLAZGOS.md (hallazgo #166).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <aether_radio/x6200_control/control.h>

/* Funcion pura: dado el nivel (0..100) decide si el squelch del DSP va
 * habilitado. 0 = abierto; cualquier nivel > 0 = habilitado. */
bool sql_should_enable(uint8_t level);

/* Aplica el nivel al DSP segun el modo (voz o FM) y deja el enable coherente
 * con la convencion de arriba. La llama el observer de cfg_cur.sql_level en
 * cada cambio y radio_init() al arrancar. */
void sql_policy_apply(x6200_mode_t mode, uint8_t level);

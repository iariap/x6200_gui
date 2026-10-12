/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - busqueda de dispositivos de entrada por nombre.
 *
 *  Los encoders se abrian por ruta fija (/dev/input/event1..4), pero el numero de eventN
 *  depende del orden de sondeo del kernel: si se enumera antes un dispositivo USB, las
 *  perillas quedan en otros nodos y el codigo no las encuentra. El device tree los llama
 *  rotary0..rotary3, y ese nombre es estable, asi que se busca por ahi.
 */

#pragma once

/*
 * Devuelve la ruta del primer /dev/input/event* cuyo nombre (EVIOCGNAME) sea `name`.
 * Si no encuentra ninguno, devuelve `fallback` (que puede ser NULL o la ruta de siempre).
 * El buffer devuelto es estatico: usarlo antes de la proxima llamada.
 */
char * input_dev_by_name(const char *name, const char *fallback);

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdint.h>

/* Superficie minima del backlight: la unica puerta de entrada del nivel del
 * panel (`backlight_set_brightness`, src/backlight.c, que escribe el PWM por
 * sysfs). Vive en su propio header porque `backlight.h` arrastra
 * `params/params.h` (LVGL + el control de la radio), que el build nativo del
 * host no tiene; los modulos que solo fijan el nivel (src/visual_policy.c) y
 * sus stubs (tests/native/stubs/stubs.c, el arnes) incluyen ESTE header y no el
 * grande. Asi el prototipo tiene una sola declaracion y no puede divergir.
 *
 * Contrato del nivel (el mismo dominio que el slider de Ajustes,
 * src/dialog_settings.c): -1 apaga el display, 0..9 es el brillo del panel y
 * 9 es el maximo. */
void backlight_set_brightness(int16_t value);

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "params/params.h"
#include "backlight_level.h"

void backlight_init();
void backlight_tick();

/* backlight_set_brightness() queda declarada en backlight_level.h: es la unica
 * salida que necesitan los modulos que no pueden incluir este header entero
 * (arrastra params/params.h). */

void backlight_set_buttons(buttons_light_t value);

void backlight_switch();
bool backlight_is_on();

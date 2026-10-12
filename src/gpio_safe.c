/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - GPIO a prueba de fallos (ver gpio_safe.h).
 */

#include "gpio_safe.h"

#include "lvgl/lvgl.h"

#include <aether_radio/x6200_control/low/gpio.h>

static bool preparado;
static bool listo;

bool gpio_safe_prepare(void) {
    if (!preparado) {
        preparado = true;
        listo     = x6200_gpio_init();

        if (listo) {
            LV_LOG_USER("GPIO listo");
        } else {
            LV_LOG_ERROR("El GPIO no se pudo inicializar: se sigue sin él (no se tocan "
                         "las líneas, que quedaron sin reservar)");
        }
    }

    return listo;
}

bool gpio_safe_ready(void) {
    return listo;
}

void gpio_safe_set(int pin, int value) {
    if (listo) {
        x6200_gpio_set(pin, value);
    }
}

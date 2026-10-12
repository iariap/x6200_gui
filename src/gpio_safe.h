/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - GPIO a prueba de fallos.
 *
 *  x6200_gpio_set() de la librería de control usa punteros internos que llena
 *  x6200_gpio_init(). Si ese init falla -por ejemplo porque el firmware base ya tiene
 *  tomada alguna de las líneas- los punteros quedan NULL y libgpiod desreferencia NULL:
 *  el proceso muere. Y la GUI lo llama desde el backlight, el wifi y el CAT, que corren
 *  temprano. Acá se inicializa una sola vez, se recuerda el resultado, y los usos se
 *  saltean si no quedó listo.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Llama a x6200_gpio_init() una sola vez y devuelve si quedó listo. */
bool gpio_safe_prepare(void);

/* Como x6200_gpio_set(), pero no hace nada si el GPIO no está listo. */
void gpio_safe_set(int pin, int value);

/* Para saber si quedó listo (por ejemplo para loguearlo). */
bool gpio_safe_ready(void);

#ifdef __cplusplus
}
#endif

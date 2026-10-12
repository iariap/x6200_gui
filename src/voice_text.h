/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - lo que la voz dice, sin el motor.
 *
 *  Acá vive la lógica que se puede probar en el host: cuándo hay que hablar (según el ajuste
 *  Off / When LCD off / Always), cómo se deletrea una frecuencia y la traducción al español de
 *  los textos que la GUI pasa en inglés.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Modos de params.voice_mode */
enum {
    VOICE_MODE_OFF = 0,
    VOICE_MODE_LCD,
    VOICE_MODE_ALWAYS,
};

/* ¿Hay que hablar? `lcd_on` es el estado de la pantalla (backlight_is_on()). */
bool voice_should_speak(int mode, bool lcd_on);

/* Una frecuencia, deletreada para que se entienda al oído: 7170000 -> "7 punto 1 7 0,
 * megahertz". Los dígitos van uno por uno porque "siete millones ciento setenta mil" no se
 * entiende en una radio. */
void voice_freq_text(uint64_t hz, char *buf, size_t n);

/* Traduce los textos de la GUI (inglés) al español con un diccionario de palabras. Lo que no
 * está en el diccionario se deja como vino. */
void voice_translate(const char *in, char *buf, size_t n);

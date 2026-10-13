/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - anuncios por voz (TTS con espeak).
 *
 *  Mismos nombres que el voice.h que traía el firmware base, así el resto del código no
 *  cambia: sólo deja de ser no-op. La voz sintetiza en un hilo propio con una cola, así que la
 *  GUI nunca espera; se respeta el ajuste Voice mode (Off / When LCD off / Always) y **no se
 *  habla mientras se transmite**.
 */

#pragma once

#ifndef X6200_VOICE_H
#define X6200_VOICE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus

#include <cstdarg>

extern "C" {
#include <stdint.h>
#endif

#define VOICES_NUM 1

typedef enum {
    VOICE_OFF = 0,
    VOICE_LCD,
    VOICE_ALWAYS
} voice_mode_t;

void voice_sure(void);

/* La GUI ya esta en pie: desde aca la voz puede hablar (ver voice.c) */
void voice_ready(void);

/* Dice $X6200_VOICE_TEST al arrancar (diagnóstico). Devuelve true si hizo la prueba, y en ese
 * caso la GUI tiene que terminar: es un diagnóstico, no una forma de arrancar. */
bool voice_selftest(void);
void voice_change_mode(void);
void voice_say_text_fmt(const char * fmt, ...);
void voice_delay_say_text_fmt(const char * fmt, ...);
void voice_say_freq(uint64_t freq);
void voice_say_bool(const char *prompt, bool x);
void voice_say_int(const char *prompt, int32_t x);
void voice_say_float(const char *prompt, float x);
void voice_say_float2(const char *prompt, float x);
void voice_say_text(const char *prompt, const char *x);
void voice_say_lang(void);
const char * voice_change(int16_t diff);

#ifdef __cplusplus
}
#endif

#endif /* X6200_VOICE_H */

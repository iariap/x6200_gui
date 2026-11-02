/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#define AUDIO_PLAY_RATE     (44100)
#define AUDIO_CAPTURE_RATE  (44100)

/* Decimation of the audio the DSP hands to the apps: 11025 Hz */
#define AUDIO_DECIM         (4)

void audio_init();
int audio_play(int16_t *buf, size_t samples);
void audio_play_wait();
void audio_play_en(bool on);

/* Prueba temporal del camino existente: audio generado por la GUI -> play_stm -> DAC.
 * Solo actua en RX; devuelve false si ya hay una prueba en curso o el estado no es RX. */
bool audio_speaker_selftest_start(void);

/* audio_init() conecta PulseAudio en un hilo: espera hasta `timeout_ms` a que el stream de
 * reproduccion este listo. Devuelve true si quedo listo. Lo usa el diagnostico de voz, que si no
 * hablaba antes de que el audio existiera (y antes eso se reportaba como "se mando", ver
 * docs/HALLAZGOS.md 2). */
bool audio_wait_ready(unsigned timeout_ms);

void audio_gain_db(int16_t *buf, size_t samples, float gain, int16_t *out);
void audio_gain_db_transition(int16_t *buf, size_t samples, float gain1, float gain2, int16_t *out);

float audio_set_play_vol(float db);
float audio_set_rec_vol(float db);

float audio_get_peak_db();

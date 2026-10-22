/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "helpers.h"
/* La instantanea espectral que consume el Auto Tune multimodo (issue #205): su
 * tipo y las politicas puras viven en src/autotune_spectrum.h, que no arrastra
 * ni `radio.h` ni `liquid/liquid.h` (asi el modulo puro se compila y se testea
 * sin esas dependencias). dsp.h la reexporta junto con el acceso del DSP. */
#include "autotune_spectrum.h"

#ifdef __cplusplus

#include "cfg/subjects.h"
#include <array>

extern "C" {
#endif

  #include "radio.h"
  #include <stdbool.h>
  #include <stdint.h>
  #include <stdlib.h>
  #include <liquid/liquid.h>

#ifdef __cplusplus
}
#endif

#define WATERFALL_NFFT  512
#define SPECTRUM_NFFT   800

#ifdef __cplusplus

extern "C" {
#endif

void dsp_init();
void dsp_samples(float *buf_samples, uint16_t size, bool tx, int16_t dbm);
void dsp_reset();

/* --- instantanea espectral del Auto Tune multimodo (issue #205) -------------
 *
 * `dsp_samples()` publica, en el hilo del DSP y sin tocar LVGL, una copia
 * coherente de la PSD promediada que ya calcula para el espectro (los mismos 800
 * valores en dB, antes del suavizado visual), junto con la secuencia, el
 * timestamp, el epoch y el contexto vigente (frecuencia, modo y ancho de FFT
 * observados). No hay FFT nueva ni cadencia nueva: es la fuente que ya existe.
 *
 * `dsp_autotune_get_spectrum()` copia la ultima publicacion bajo su propio mutex
 * (solo la copia; nunca se retiene para procesar ni para dibujar) y devuelve true
 * si hay alguna publicacion. El llamador mira `out->valid` para saber si esa
 * trama sirve: TX, ancho no positivo o un valor no finito la dejan invalida.
 *
 * `dsp_autotune_invalidate()` sube el epoch y descarta la publicacion vigente:
 * la llama el GUI antes de una escritura propia del dial y en los cambios de
 * VFO/frecuencia/modo/ancho/TX-RX, para que ninguna trama anterior al cambio
 * pueda mezclarse con las posteriores. */
bool dsp_autotune_get_spectrum(dsp_autotune_spectrum_t *out);
void dsp_autotune_invalidate(void);

float dsp_get_spectrum_beta();
void dsp_set_spectrum_beta(float x);

/* The X6100 apps hide the spectrum and the waterfall while they run. In this
 * GUI an app is a full dialog drawn over the main screen, so there is nothing
 * to hide; the calls are accepted and ignored. */
void dsp_set_spectrum_enabled(bool enabled);
void dsp_set_waterfall_enabled(bool enabled);

void dsp_put_audio_samples(size_t nsamples, int16_t *samples);
#ifdef __cplusplus
}
#endif

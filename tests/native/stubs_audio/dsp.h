/*
 * Consumers/routing stubs for the capture-callback stage (issue #351, parte 2).
 *
 * src/audio.c incluye "dsp.h", "meter.h", "screen_rec.h" y "audio_enhancement.h"; los
 * reales arrastran LVGL/liquid/el SDK de la radio y no hacen falta para probar el
 * callback. Estas tres cabeceras minimas declaran SOLO lo que el callback llama, y las
 * implementaciones (tests/native/audio_callback_stubs.c) registran orden y contenido de
 * cada entrega.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

void dsp_init(void);
void dsp_put_audio_samples(size_t nsamples, int16_t *samples);

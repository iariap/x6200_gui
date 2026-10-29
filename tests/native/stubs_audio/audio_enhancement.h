/*
 * Audio enhancement stub for the capture-callback stage (issue #351, parte 2).
 *
 * El callback solo puede usar dos entradas: audio_enh_process() (PCM real, antes de los
 * consumidores) y audio_enh_nr_reset_pending() (hole). El stub imita al modulo real en lo
 * que el test observa: process() escribe IN PLACE (como Speex) y deja el estado NR
 * "sucio"; el reset lo limpia. Asi un hole que reusara audio viejo se ve en el canario.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

void audio_enh_init(void);
void audio_enh_process(int16_t *samples, size_t nsamples);
void audio_enh_nr_reset_pending(void);

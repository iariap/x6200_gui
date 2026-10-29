/*
 * Screen recorder stub for the capture-callback stage (issue #351, parte 2): el
 * consumidor real guarda el PCM en su ring; el stub registra el orden y una copia.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

void screen_rec_put_audio_samples(size_t nsamples, int16_t *samples);

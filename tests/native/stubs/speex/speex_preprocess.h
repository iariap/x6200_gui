/*
 * Minimal SpeexDSP preprocessor stub for the unit tests.
 * Deterministic behaviour (halves every sample) so the tests can prove that
 * noise reduction only runs on complete AUDIO_ENH_FRAME_SIZE frames.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef struct SpeexPreprocessState_ SpeexPreprocessState;

#define SPEEX_PREPROCESS_SET_DENOISE 0
#define SPEEX_PREPROCESS_SET_NOISE_SUPPRESS 1

SpeexPreprocessState *speex_preprocess_state_init(int frame_size, int sampling_rate);
void speex_preprocess_state_destroy(SpeexPreprocessState *st);
int  speex_preprocess_ctl(SpeexPreprocessState *st, int request, void *ptr);
int  speex_preprocess_run(SpeexPreprocessState *st, int16_t *x);

/* Test helpers */
int  test_speex_frames(void);

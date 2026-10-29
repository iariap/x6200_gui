/*
 * Minimal SpeexDSP preprocessor stub for the unit tests.
 *
 * Deterministic behaviour (halves every sample) so the tests can prove that
 * noise reduction only runs on complete AUDIO_ENH_FRAME_SIZE frames and that
 * the frame machine is continuous across callbacks. This stub proves NOTHING
 * acoustic: no attenuation, no CPU and no latency claim may be based on it.
 *
 * The id below is the REAL libspeexdsp 1.2.1 ABI value (speex_preprocess.h), not
 * a stub-local number: with a wrong id the module could call
 * SPEEX_PREPROCESS_SET_NOISE_SUPPRESS and the real library would ignore it
 * silently, so the host test would stay green while the radio never changed the
 * suppression level.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef struct SpeexPreprocessState_ SpeexPreprocessState;

#define SPEEX_PREPROCESS_SET_DENOISE 0
#define SPEEX_PREPROCESS_SET_NOISE_SUPPRESS 18

SpeexPreprocessState *speex_preprocess_state_init(int frame_size, int sampling_rate);
void speex_preprocess_state_destroy(SpeexPreprocessState *st);
int  speex_preprocess_ctl(SpeexPreprocessState *st, int request, void *ptr);
int  speex_preprocess_run(SpeexPreprocessState *st, int16_t *x);

/* Test helpers */
int  test_speex_frames(void);
int  test_speex_init_count(void);
int  test_speex_init_frame_size(void);
int  test_speex_init_rate(void);
int  test_speex_init_mismatch(void);
int  test_speex_noise_suppress(void);
int  test_speex_noise_suppress_calls(void);
void test_speex_reset_counters(void);

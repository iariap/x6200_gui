/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Fine-tuning assistant (autotune)
 *
 *  Copyright (c) 2024 LU2DSX
 *
 *  Deterministic, UI-free core of the operator's fine-tuning assistant
 *  (issue #100): given the start frequency it plans a bounded zigzag of
 *  probes around it, scores the signal quality measured at each probe and
 *  decides the final frequency. When no probe beats the start by a margin it
 *  returns the start, so the owner's dial is never lost.
 *
 *  The signal the caller feeds is the S-meter level the radio reports
 *  (`meter_get_raw_db()`, in dBm): the RF level of the DSP packet
 *  (src/radio.c -> dsp_samples -> update_s_meter -> meter_update), not the
 *  audio level, which is post-gain and would move with the volume knob. See
 *  docs/autotune.md. This module does no I/O, no LVGL and no radio access, so
 *  the whole algorithm is exercised by the native tests with synthetic
 *  fixtures.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Calibration (defaults; the caller may override step/window in autotune_plan):
 *  - STEP_HZ: one S-meter step is a fraction of a kHz. 500 Hz is the finest
 *    grid the dial reaches at 1 Hz resolution without aliasing the S-meter
 *    smoothing window, and it is the "1 bin" the issue asks for.
 *  - WINDOW_HZ: +/- 5 kHz around the start, as the issue specifies. */
#define AUTOTUNE_WINDOW_HZ      5000
#define AUTOTUNE_STEP_HZ        500
#define AUTOTUNE_SAMPLES        4       /* k samples per probe (median-scored) */
#define AUTOTUNE_MIN_IMPROVE_DB 1       /* best must beat the start by this */
#define AUTOTUNE_MAX_PROBES     64

/* Number of samples a single score is computed from (autotune_score clamps to
 * this; the buffer is fixed so there is no allocation in the scan). */
#define AUTOTUNE_MAX_SAMPLES    16

typedef struct {
    int32_t  start_freq;                     /* the dial at start (the anchor) */
    int32_t  step_hz;
    int32_t  window_hz;
    int      count;                          /* probes planned */
    int      index;                          /* probe being measured */
    int      collected;                      /* samples fed to the current probe */
    int      completed;                      /* probes whose score is stored */
    int32_t  probes[AUTOTUNE_MAX_PROBES];    /* absolute frequencies, zigzag */
    int16_t  scores[AUTOTUNE_MAX_PROBES];    /* robust score per completed probe */
    int16_t  samples[AUTOTUNE_MAX_SAMPLES];  /* samples of the current probe */
} autotune_t;

/* Robust score of `n` samples: the median (an outlier spike is rejected).
 * Returns INT16_MIN for n <= 0. */
int16_t autotune_score(const int16_t *samples, int n);

/* Plan the probes: start, then +step, -step, +2*step, -2*step, ... while the
 * offset stays within +/-window. Returns the number of probes (>= 1).
 * step_hz <= 0 and window_hz < 0 fall back to the defaults. */
int autotune_plan(autotune_t *at, int32_t start_freq, int32_t step_hz, int32_t window_hz);

/* The absolute frequency of the probe being measured. */
int32_t autotune_probe_freq(const autotune_t *at);

/* Feed one signal sample to the current probe. Returns true when the probe is
 * complete (its score has been stored and `completed` advanced). */
bool autotune_feed(autotune_t *at, int16_t rssi);

/* Advance to the next probe. Returns false when the plan is exhausted. */
bool autotune_next(autotune_t *at);

/* True while there are probes left to measure. */
bool autotune_running(const autotune_t *at);

/* Final frequency: the best probe if it beats the start by `margin_db`, else
 * the start frequency (deterministic restore; the dial is never lost). */
int32_t autotune_result(const autotune_t *at, int16_t margin_db);

#ifdef __cplusplus
}
#endif

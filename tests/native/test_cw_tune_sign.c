/*
 * Sign test for Auto Tune v2 (issue #190): a synthetic complex CW tone through
 * the SAME DSP chain src/cw.cpp runs - DDS mixer + decimation, Hann window,
 * 128-point FFT, argmax, cw_peak_freq_from_bin() - and then the normalized error
 * cw_tune_error_hz(). This is what fixes the orientation: a tone ABOVE the
 * configured pitch must give a POSITIVE error_hz (and therefore +35 Hz of dial
 * in CW, -35 Hz in CWR through autotune_dial_delta()).
 *
 * It exists because a test that injects an already-normalized error proves
 * nothing about the orientation; the FFT formula returns the NEGATIVE of the
 * tone's offset (docs/HALLAZGOS.md 52) and hiding that behind a hand-made
 * fixture is exactly the failure mode the issue forbids.
 *
 * The mode-dependent relation between the dial and the audio tone is the radio's
 * demodulation (CW: audio = signal - dial; CWR, inverted sideband:
 * audio = dial - signal). It is modelled here, in the test, and marked as
 * requires-radio: what this file proves is the DSP/FFT sign and the closed loop
 * with that model.
 *
 * Needs liquid-dsp (the same 1.4.0 the firmware links). The native runner skips
 * it when the host has no liquid/liquid.h; the x6200-uiharness image has it.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <liquid/liquid.h>

#include "autotune.h"
#include "cw.h"
#include "test_framework.h"

/* Los mismos numeros que src/cw.cpp. */
#define AUDIO_RATE   44100.0f
#define NUM_STAGES   6
#define DECIM        (1 << NUM_STAGES)          /* 64 */
#define FFT_N        128
#define MAX_CW_BW    500.0f
#define DECIM_RATE   (AUDIO_RATE / (float) DECIM)     /* 689,0625 Hz */
#define BIN_HZ       (DECIM_RATE / (float) FFT_N)     /* 5,383 Hz */

/* Error normalizado que el DSP publica para un tono de audio `audio_tone_hz`
 * (frecuencia compleja positiva = audio audible) con el pitch y el corrimiento
 * del DDS dados, medido con la cadena real. */
static float measure_error_hz(float audio_tone_hz, float pitch_hz, float dds_offset_hz) {
    complex float *in;
    complex float  out[FFT_N];
    complex float  time_buf[FFT_N];
    complex float  freq_buf[FFT_N];
    float          window[FFT_N];
    float          psd[FFT_N];
    float          scale = 0.0f;
    fftplan        plan;
    dds_cccf       ds;
    int            best = 0;
    float          best_val = -1.0f;
    float          peak_freq;
    float          error;

    /* DDS: mezcla al centro (pitch + offset) y decima por 64, igual que
     * cw.cpp (rel_freq y bw normalizados a la tasa de audio). */
    ds = dds_cccf_create(NUM_STAGES, (pitch_hz + dds_offset_hz) / AUDIO_RATE,
                         MAX_CW_BW / AUDIO_RATE, 60.0f);

    in = malloc(sizeof(*in) * (size_t) (DECIM * FFT_N));
    if (in == NULL) {
        fprintf(stderr, "sign test: sin memoria\n");
        exit(1);
    }
    for (int i = 0; i < DECIM * FFT_N; i++) {
        double ph = 2.0 * M_PI * (double) audio_tone_hz * (double) i / (double) AUDIO_RATE;
        in[i] = (float) cos(ph) + I * (float) sin(ph);
    }
    for (int b = 0; b < FFT_N; b++) {
        dds_cccf_decim_execute(ds, in + b * DECIM, &out[b]);
    }

    for (int i = 0; i < FFT_N; i++) {
        window[i] = liquid_hann(i, FFT_N);
        scale += window[i] * window[i];
    }
    scale = 1.0f / sqrtf(scale);
    for (int i = 0; i < FFT_N; i++) {
        time_buf[i] = out[i] * (window[i] * scale);
    }

    plan = fft_create_plan(FFT_N, time_buf, freq_buf, LIQUID_FFT_FORWARD, 0);
    fft_execute(plan);
    fft_destroy_plan(plan);

    for (int i = 0; i < FFT_N; i++) {
        psd[i] = crealf(freq_buf[i] * conjf(freq_buf[i]));
        if (psd[i] > best_val) {
            best_val = psd[i];
            best = i;
        }
    }

    peak_freq = cw_peak_freq_from_bin(best, FFT_N, DECIM_RATE);
    error = cw_tune_error_hz(peak_freq, dds_offset_hz);

    dds_cccf_destroy(ds);
    free(in);
    return error;
}

/* Tolerancia: el estimador es un bin (5,383 Hz) mas los lados de los lobulos. */
#define TOL_HZ  8.0f

static void test_dsp_error_orientation(void) {
    float pitch = 700.0f;
    float above = measure_error_hz(pitch + 35.0f, pitch, 0.0f);
    float below = measure_error_hz(pitch - 35.0f, pitch, 0.0f);
    float on_pitch = measure_error_hz(pitch, pitch, 0.0f);

    /* Tono POR ENCIMA del pitch -> error POSITIVO (el crudo de la FFT es negativo) */
    CHECK(above > 0.0f);
    CHECK_CLOSE(above, 35.0f, TOL_HZ);
    CHECK(below < 0.0f);
    CHECK_CLOSE(below, -35.0f, TOL_HZ);
    CHECK_CLOSE(on_pitch, 0.0f, TOL_HZ);
}

static void test_dsp_compensates_the_dds_offset(void) {
    float pitch = 700.0f;
    float above = measure_error_hz(pitch + 35.0f, pitch, -30.0f);
    float below = measure_error_hz(pitch - 35.0f, pitch, 45.0f);

    /* El error se mide contra el PITCH manual, no contra el centro desplazado por
     * el AUTO del decodificador (#139): el offset no cambia ni el valor ni el signo. */
    CHECK(above > 0.0f);
    CHECK_CLOSE(above, 35.0f, TOL_HZ);
    CHECK(below < 0.0f);
    CHECK_CLOSE(below, -35.0f, TOL_HZ);
}

static void test_dsp_tracks_the_pitch_setting(void) {
    /* El pitch objetivo es el configurado: un tono a 700 Hz con pitch 720 da un
     * error negativo de -20 (no se asume un target fijo de 700). */
    float err = measure_error_hz(700.0f, 720.0f, 0.0f);

    CHECK(err < 0.0f);
    CHECK_CLOSE(err, -20.0f, TOL_HZ);
}

/* --- ciclo cerrado, tono fisicamente 35 Hz arriba del pitch ---------------- */

/* Modelo del receptor: con el dial en `dial`, un tono de audio A sale de la
 * senal en `signal` segun la banda lateral. Devuelve el error medido. */
static float closed_loop(float signal_hz, int32_t dial, float pitch,
                         autotune_mode_t mode) {
    float audio;

    if (mode == AUTOTUNE_MODE_CW) {
        audio = signal_hz - (float) dial;          /* banda lateral normal */
    } else {
        audio = (float) dial - signal_hz;          /* banda lateral invertida */
    }
    return measure_error_hz(audio, pitch, 0.0f);
}

static void test_closed_loop_cw_and_cwr(void) {
    float pitch = 700.0f;
    int32_t dial0 = 14074000;

    for (int m = 0; m < 2; m++) {
        autotune_mode_t mode = (m == 0) ? AUTOTUNE_MODE_CW : AUTOTUNE_MODE_CWR;
        /* La senal fisica esta 35 Hz por encima del pitch EN EL AIRE: en CW eso es
         * dial + pitch + 35; en CWR el dial queda arriba de la senal. Las dos
         * suenan 35 Hz mas agudo que el pitch. */
        float signal_hz = (mode == AUTOTUNE_MODE_CW)
                              ? (float) dial0 + pitch + 35.0f
                              : (float) dial0 - pitch - 35.0f;
        float error = closed_loop(signal_hz, dial0, pitch, mode);
        int32_t delta;
        float after;

        CHECK(error > 0.0f);
        CHECK_CLOSE(error, 35.0f, TOL_HZ);

        /* La correccion es el error REDONDEADO, con el signo del modo: + en CW,
         * - en CWR (el estimador cuantiza a un bin de 5,383 Hz, asi que la
         * comprobacion es contra el error medido, no contra 35 clavado). */
        delta = autotune_dial_delta(error, mode);
        CHECK(delta == (int32_t) roundf(error) *
                           ((mode == AUTOTUNE_MODE_CW) ? 1 : -1));
        CHECK((delta > 0) == (mode == AUTOTUNE_MODE_CW));
        CHECK((delta < 0) == (mode == AUTOTUNE_MODE_CWR));

        /* Con el dial corregido, el error medido vuelve a ~0: la correccion de una
         * sola pasada centra el tono (dentro de la banda muerta de 10 Hz). */
        after = closed_loop(signal_hz, dial0 + delta, pitch, mode);
        CHECK(fabsf(after) <= AUTOTUNE_DEADBAND_HZ);
    }
}

void test_cw_tune_sign(void) {
    test_dsp_error_orientation();
    test_dsp_compensates_the_dds_offset();
    test_dsp_tracks_the_pitch_setting();
    test_closed_loop_cw_and_cwr();
}

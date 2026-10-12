/*
 * Unit tests: WSPR encoder, synthesizer and decoder (round trip).
 *
 * Encodes a message, synthesizes the 4-FSK audio, feeds it through the baseband
 * recorder at 11.025 kHz (the rate dsp.cpp hands to the apps) and decodes it
 * back with K1JT's wsprd engine, checking the callsign, locator, power and RF
 * frequency. Also checks the power mapping and the messages WSPR cannot send.
 *
 * Needs libfftw3f (float FFT), which the firmware image already ships; run it
 * with tests/native/run_wspr_tests.sh, which compiles it in a container that has
 * the development headers.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_framework.h"
#include "wspr/wspr.h"

#define AUDIO_RATE  11025
#define CYCLE_S     116

/* Deterministic PRNG, so a failure is reproducible */
static uint32_t rng_state = 1;

static float rng_uniform(void) {
    rng_state = rng_state * 1103515245u + 12345u;

    return (float) ((rng_state >> 8) & 0xFFFFFF) / (float) 0x1000000;
}

static float rng_gauss(void) {
    float u1 = rng_uniform() + 1e-7f;
    float u2 = rng_uniform();

    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float) M_PI * u2);
}

/* One WSPR cycle of RX audio: the signal starts 1 s after the even minute */
static float *make_cycle(const char *call, const char *grid, int dbm, float audio_hz, float snr_db) {
    uint8_t  sym[WSPR_SYMBOLS];
    uint32_t n  = 0;
    size_t   sz = (size_t) AUDIO_RATE * CYCLE_S;
    float   *audio = calloc(sz, sizeof(float));

    CHECK(audio != NULL);
    CHECK(wspr_encode(call, grid, dbm, sym));

    int16_t *tx = wspr_synth(sym, audio_hz, AUDIO_RATE, &n);

    CHECK(tx != NULL);

    if (!tx || !audio) {
        free(tx);
        free(audio);
        return NULL;
    }

    /* WSPR SNR is defined in 2500 Hz, the noise power here is over 5512 Hz */
    float sig_rms = 0.8f / sqrtf(2.0f);
    float noise   = sig_rms / powf(10.0f, snr_db / 20.0f) * sqrtf((float) AUDIO_RATE / 2.0f / 2500.0f);

    rng_state = 1;

    for (size_t k = 0; k < sz; k++) {
        float s = 0.0f;

        if (k >= (size_t) AUDIO_RATE && k - AUDIO_RATE < n) {
            s = tx[k - AUDIO_RATE] / 32767.0f;
        }

        audio[k] = 0.1f * (s + rng_gauss() * noise);
    }

    free(tx);

    return audio;
}

static int decode(const float *audio, size_t sz, uint32_t dial_hz, wspr_spot_t *spots, int max) {
    wspr_bb_t *bb = wspr_bb_create(AUDIO_RATE);
    float     *i  = calloc(WSPR_SAMPLES, sizeof(float));
    float     *q  = calloc(WSPR_SAMPLES, sizeof(float));

    CHECK(bb != NULL);
    CHECK(i != NULL);
    CHECK(q != NULL);

    if (!bb || !i || !q) {
        wspr_bb_destroy(bb);
        free(i);
        free(q);
        return 0;
    }

    for (size_t k = 0; k < sz; k += 441) {
        unsigned chunk = (sz - k) < 441 ? (unsigned) (sz - k) : 441;

        wspr_bb_feed(bb, &audio[k], chunk);
    }

    unsigned n = wspr_bb_take(bb, i, q);

    wspr_bb_destroy(bb);

    /* A full cycle at the baseband rate, allow a little slack */
    CHECK(n > 115 * WSPR_RATE - 10);

    int found = wspr_decode_iq(i, q, n, dial_hz, spots, max);

    free(i);
    free(q);

    return found;
}

/* Encode -> audio -> baseband -> decode at a comfortable SNR */
static void test_round_trip(void) {
    wspr_spot_t spots[10];
    float      *audio = make_cycle("IU1WXY", "JN35", 37, 1530.0f, -20.0f);

    CHECK(audio != NULL);

    if (!audio) {
        return;
    }

    int n = decode(audio, (size_t) AUDIO_RATE * CYCLE_S, 14095600, spots, 10);

    CHECK(n >= 1);

    if (n >= 1) {
        CHECK(strcmp(spots[0].call, "IU1WXY") == 0);
        CHECK(strcmp(spots[0].loc, "JN35") == 0);
        CHECK(spots[0].dbm == 37);
        /* The spot frequency is the dial plus the audio offset (1530 Hz) */
        CHECK(fabs(spots[0].freq_hz - 14097130.0) < 20.0);
        printf("  spot: %s %s %d dBm, %.1f Hz, snr %d, dt %.2f\n",
               spots[0].call, spots[0].loc, spots[0].dbm, spots[0].freq_hz, spots[0].snr, spots[0].dt);
    }

    free(audio);
}

/* A second message, weaker and offset, to make sure it is not a fluke */
static void test_round_trip_weak(void) {
    wspr_spot_t spots[10];
    float      *audio = make_cycle("DL1ABC", "JO31", 23, 1450.0f, -25.0f);

    if (!audio) {
        return;
    }

    int n = decode(audio, (size_t) AUDIO_RATE * CYCLE_S, 7040000, spots, 10);

    CHECK(n >= 1);

    if (n >= 1) {
        CHECK(strcmp(spots[0].call, "DL1ABC") == 0);
        CHECK(spots[0].dbm == 23);
    }

    free(audio);
}

/* Noise only: the decoder must not invent a spot */
static void test_noise_only(void) {
    size_t   sz    = (size_t) AUDIO_RATE * CYCLE_S;
    float   *audio = calloc(sz, sizeof(float));
    wspr_spot_t spots[10];

    rng_state = 7;

    for (size_t k = 0; k < sz; k++) {
        audio[k] = 0.1f * rng_gauss();
    }

    int n = decode(audio, sz, 14095600, spots, 10);

    CHECK(n == 0);

    free(audio);
}

/* The levels WSPR can actually transmit */
static const int wspr_levels[] = { 0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40, 43, 47, 50, 53, 57, 60 };

static bool is_valid_level(int d) {
    for (size_t k = 0; k < sizeof(wspr_levels) / sizeof(wspr_levels[0]); k++) {
        if (wspr_levels[k] == d) {
            return true;
        }
    }

    return false;
}

static void test_power_mapping(void) {
    CHECK(wspr_dbm(0.001f) == 0);       /* 1 mW */
    CHECK(wspr_dbm(0.005f) == 7);       /* 5 mW */
    CHECK(wspr_dbm(1.0f) == 30);        /* 1 W */
    CHECK(wspr_dbm(5.0f) == 37);        /* 5 W */
    CHECK(wspr_dbm(10.0f) == 40);
    CHECK(wspr_dbm(100.0f) == 50);

    /* Any real power maps to a level WSPR can send, and the mapping is
     * monotonic (more watts never means fewer dBm) */
    int prev = -1;

    for (float w = 0.001f; w < 2000.0f; w *= 1.3f) {
        int d = wspr_dbm(w);

        CHECK(is_valid_level(d));
        CHECK(d >= prev);
        prev = d;
    }

    /* Below 1 mW it clamps to the lowest level */
    CHECK(wspr_dbm(0.0f) == 0);
    CHECK(wspr_dbm(-1.0f) == 0);
}

static void test_encode_limits(void) {
    uint8_t sym[WSPR_SYMBOLS];

    /* Type 1 messages: a compound call cannot be sent */
    CHECK(wspr_encode("IU1WXY", "JN35", 37, sym));
    CHECK(!wspr_encode("DL/IU1WXY", "JN35", 37, sym));
    CHECK(wspr_encode("K1ABC", "FN42", 30, sym));
    /* A full locator is valid, half a locator is not */
    CHECK(wspr_encode("K1ABC", "FN42AB", 30, sym));
    CHECK(!wspr_encode("K1ABC", "FN", 30, sym));
}

/* The baseband recorder must report how much it holds and survive a reset */
static void test_baseband_recorder(void) {
    wspr_bb_t *bb = wspr_bb_create(AUDIO_RATE);
    float     *i  = calloc(WSPR_SAMPLES, sizeof(float));
    float     *q  = calloc(WSPR_SAMPLES, sizeof(float));
    float      chunk[441];

    CHECK(bb && i && q);

    if (!bb || !i || !q) {
        return;
    }

    for (size_t k = 0; k < 441; k++) {
        chunk[k] = 0.0f;
    }

    /* 110 s of audio decimates to about 110/120 of a cycle */
    for (int s = 0; s < 110 * (AUDIO_RATE / 441); s++) {
        wspr_bb_feed(bb, chunk, 441);
    }

    unsigned n = wspr_bb_take(bb, i, q);

    CHECK(n > WSPR_SAMPLES * 8 / 10);
    CHECK(n <= WSPR_SAMPLES);

    /* Feeding more than a cycle must not overflow the buffers */
    for (int s = 0; s < 20 * (AUDIO_RATE / 441); s++) {
        wspr_bb_feed(bb, chunk, 441);
    }

    CHECK(wspr_bb_take(bb, i, q) <= WSPR_SAMPLES);

    wspr_bb_reset(bb);
    CHECK(wspr_bb_take(bb, i, q) == 0);

    wspr_bb_destroy(bb);
    free(i);
    free(q);
}

void test_wspr(void) {
    test_power_mapping();
    test_encode_limits();
    test_baseband_recorder();
    test_round_trip();
    test_round_trip_weak();
    test_noise_only();
}

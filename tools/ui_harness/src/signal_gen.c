/*
 *  Deterministic spectrum/waterfall data for the host UI harness.
 *
 *  On the radio the FFT (dsp.cpp) pushes magnitudes into spectrum_data() and
 *  waterfall_data(); the harness has no SDR, so it synthesises the magnitudes
 *  from the scenario's seed. The generators below are pure integer/float math
 *  with no clock, no random device and no shared state: the same seed always
 *  produces the same bytes, which is what keeps the whole render deterministic.
 *
 *  This is why spectrum.c and waterfall.c are declared PARTIAL: the components
 *  are real, the data feeding them is substituted.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "signal_gen.h"

#include "spectrum.h"
#include "waterfall.h"

#define SPECTRUM_LEN 800        /* spectrum.c's SPECTRUM_SIZE */
#define WATERFALL_LEN 512       /* dsp.h's WATERFALL_NFFT */

/* Waterfall rows to push: enough to fill the ~180 px tall column with texture. */
#define WATERFALL_ROWS SIGNAL_GEN_WATERFALL_ROWS

/* A tiny deterministic PRNG (LCG): no libc rand(), no seed from the clock. */
typedef struct {
    uint32_t state;
} rng_t;

static uint32_t rng_next(rng_t *r)
{
    r->state = r->state * 1664525u + 1013904223u;
    return r->state >> 8;
}

/* One magnitude line in dB, in the band the grid shows (-120..-40 dB). */
static void fill_line(float *out, int n, uint32_t seed, int row)
{
    rng_t r = { seed * 2654435761u + (uint32_t) row * 40503u + 1u };

    /* Three carriers whose positions move with the seed (a fixed picture). */
    int c1 = (int) (n * (0.20f + (seed % 31) / 100.0f));
    int c2 = n / 2 + (int) (seed % 17) - 8;
    int c3 = (int) (n * (0.74f + (seed % 13) / 100.0f));

    for (int i = 0; i < n; i++) {
        /* Noise floor: about -114 dB, a few tenths of a dB of texture. */
        float db = -114.0f + (float) (rng_next(&r) % 40) / 10.0f;

        /* Triangular carriers (no transcendentals: libm-independent). */
        int   d1 = i - c1, d2 = i - c2, d3 = i - c3;
        float a1 = d1 < 0 ? -d1 : d1;
        float a2 = d2 < 0 ? -d2 : d2;
        float a3 = d3 < 0 ? -d3 : d3;

        if (a1 < n / 60.0f) db += 62.0f * (1.0f - a1 / (n / 60.0f));
        if (a2 < n / 40.0f) db += 50.0f * (1.0f - a2 / (n / 40.0f));
        if (a3 < n / 70.0f) db += 40.0f * (1.0f - a3 / (n / 70.0f));

        if (db > -40.0f) db = -40.0f;
        if (db < -120.0f) db = -120.0f;

        out[i] = db;
    }
}

void signal_gen_feed(uint32_t seed)
{
    float line[SPECTRUM_LEN];

    /* The spectrum: one settled frame. */
    fill_line(line, SPECTRUM_LEN, seed, 0);
    spectrum_data(line, SPECTRUM_LEN, false);

    /* The waterfall: a column of frames, each a touch different, so the
     * waterfall has the moving texture it has on the radio. */
    for (int row = 0; row < WATERFALL_ROWS; row++) {
        fill_line(line, WATERFALL_LEN, seed, row);
        waterfall_data(line, WATERFALL_LEN, false);
    }
}

void signal_gen_spectrum_frame(uint32_t seed, int frame)
{
    float line[SPECTRUM_LEN];

    /* Row 0 is the settled frame above; the natural drive moves on from it. */
    fill_line(line, SPECTRUM_LEN, seed, frame + 1);
    spectrum_data(line, SPECTRUM_LEN, false);
}

void signal_gen_waterfall_row(uint32_t seed, int row)
{
    float line[WATERFALL_LEN];

    /* The same row space: the settled column covers 0..WATERFALL_ROWS-1. */
    fill_line(line, WATERFALL_LEN, seed, WATERFALL_ROWS + row);
    waterfall_data(line, WATERFALL_LEN, false);
}

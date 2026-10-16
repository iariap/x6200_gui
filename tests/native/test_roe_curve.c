/*
 * Native test for src/roe_curve.c (issue #99: the ROE/SWR scan chart drew the
 * raw samples as straight segments -- a sawtooth -- and the owner asked for a
 * smoothed curve).
 *
 * The render itself lives in dialog_swrscan.c's draw callback, which only the
 * panel draws; what is pinned here is the geometry source it feeds the chart:
 * roe_curve_resample() turns the N samples into a smooth curve. The product
 * safeguard of the issue is that the local minima (the antenna resonances, the
 * spots worth putting an antenna on) survive as anchors of the curve, while the
 * noise between them is what gets rounded off.
 *
 * The invariants this test pins:
 *   - the first and last samples are the curve's endpoints;
 *   - every sample is an exact knot of the curve (a local minimum cannot be
 *     smoothed away);
 *   - no overshoot: the curve never dips below a resonance nor rises above a
 *     sample, so the smoothing does not invent signal;
 *   - values are clamped to the chart's SWR range;
 *   - the result is deterministic.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "test_framework.h"

#include "roe_curve.h"

#include <string.h>

/* Enough for any input this test feeds the resampler (n <= 64). */
#define MAXOUT ((ROE_CURVE_OVERSAMPLE + 1) * 64)

static float out[MAXOUT];
static float out2[MAXOUT];

static int out_min_index(const float *v, int n) {
    int best = 0;

    for (int i = 1; i < n; i++) {
        if (v[i] < v[best]) {
            best = i;
        }
    }

    return best;
}

static int out_max_index(const float *v, int n) {
    int best = 0;

    for (int i = 1; i < n; i++) {
        if (v[i] > v[best]) {
            best = i;
        }
    }

    return best;
}

void test_roe_curve(void) {
    /* --- el conteo de puntos: un punto por sub-muestra, los N nudos incluidos --- */
    CHECK(roe_curve_point_count(0) == 0);
    CHECK(roe_curve_point_count(1) == 1);
    CHECK(roe_curve_point_count(2) == ROE_CURVE_OVERSAMPLE + 1);
    CHECK(roe_curve_point_count(50) == (50 - 1) * ROE_CURVE_OVERSAMPLE + 1);

    /* --- extremos conservados y densidad: mas puntos que muestras --- */
    {
        float in[5] = {3.0f, 2.0f, 1.5f, 2.5f, 4.0f};
        int   m = roe_curve_point_count(5);

        roe_curve_resample(in, 5, out);

        CHECK(m > 5);
        CHECK_CLOSE(out[0], in[0], 1e-6f);          /* primer punto == primera muestra */
        CHECK_CLOSE(out[m - 1], in[4], 1e-6f);      /* ultimo punto == ultima muestra */

        /* Cada muestra es un nudo exacto de la curva. */
        for (int k = 0; k < 5; k++) {
            CHECK_CLOSE(out[k * ROE_CURVE_OVERSAMPLE], in[k], 1e-5f);
        }
    }

    /* --- la salvaguarda de producto: el minimo local queda anclado --- */
    {
        /* Una resonancia (SWR bajo) sobre un fondo de ruido. */
        float in[9] = {4.0f, 3.4f, 3.9f, 1.25f, 3.7f, 3.3f, 3.8f, 3.5f, 4.2f};
        int   m  = roe_curve_point_count(9);

        roe_curve_resample(in, 9, out);

        /* El minimo de la curva ES el minimo de la muestra (nudo exacto). */
        CHECK_CLOSE(out[3 * ROE_CURVE_OVERSAMPLE], 1.25f, 1e-5f);
        CHECK_CLOSE(out[out_min_index(out, m)], 1.25f, 1e-5f);

        /* El suavizado no inventa un dip mas profundo que la resonancia real. */
        for (int i = 0; i < m; i++) {
            CHECK(out[i] >= 1.25f - 1e-5f);
        }

        /* El indice del minimo de la curva cae en la vecindad del nudo anclado. */
        int mi = out_min_index(out, m);
        CHECK(mi >= 3 * ROE_CURVE_OVERSAMPLE - ROE_CURVE_OVERSAMPLE);
        CHECK(mi <= 3 * ROE_CURVE_OVERSAMPLE + ROE_CURVE_OVERSAMPLE);
    }

    /* --- sin overshoot: un pico no se amplifica (Catmull-Rom lo haria) --- */
    {
        float in[7] = {2.0f, 2.2f, 2.1f, 4.5f, 2.3f, 2.0f, 2.4f};
        int   m  = roe_curve_point_count(7);

        roe_curve_resample(in, 7, out);

        /* El pico queda exacto en su nudo y la curva no lo supera. */
        CHECK_CLOSE(out[3 * ROE_CURVE_OVERSAMPLE], 4.5f, 1e-5f);
        CHECK_CLOSE(out[out_max_index(out, m)], 4.5f, 1e-5f);

        for (int i = 0; i < m; i++) {
            CHECK(out[i] <= 4.5f + 1e-5f);
        }
    }

    /* --- sin overshoot en una rampa monotona: la curva tambien es monotona --- */
    {
        float in[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        int   m  = roe_curve_point_count(5);

        roe_curve_resample(in, 5, out);

        for (int i = 1; i < m; i++) {
            CHECK(out[i] >= out[i - 1] - 1e-5f);
            CHECK(out[i] <= out[i - 1] + 1.0f + 1e-5f);     /* paso acotado */
        }
    }

    /* --- clamp al rango del chart: SWR 1..5 --- */
    {
        float hi[3] = {8.0f, 12.0f, 20.0f};
        int   m     = roe_curve_point_count(3);

        roe_curve_resample(hi, 3, out);

        for (int i = 0; i < m; i++) {
            CHECK(out[i] <= ROE_CURVE_MAX_VALUE + 1e-6f);
            CHECK(out[i] >= ROE_CURVE_MIN_VALUE - 1e-6f);
        }

        float lo[3] = {0.2f, 0.0f, -1.0f};

        roe_curve_resample(lo, 3, out);

        for (int i = 0; i < m; i++) {
            CHECK(out[i] >= ROE_CURVE_MIN_VALUE - 1e-6f);
            CHECK(out[i] <= ROE_CURVE_MAX_VALUE + 1e-6f);
        }
    }

    /* --- determinismo: misma entrada, misma salida --- */
    {
        float in[11] = {3.0f, 3.2f, 2.8f, 2.9f, 1.1f, 2.7f, 3.1f, 3.0f, 3.3f, 2.6f, 3.0f};
        int   m  = roe_curve_point_count(11);

        roe_curve_resample(in, 11, out);
        roe_curve_resample(in, 11, out2);

        CHECK(memcmp(out, out2, (size_t)m * sizeof(float)) == 0);
    }

    /* --- caso degenerado: una sola muestra no rompe --- */
    {
        float in[1] = {2.5f};

        roe_curve_resample(in, 1, out);

        CHECK_CLOSE(out[0], 2.5f, 1e-6f);
    }
}

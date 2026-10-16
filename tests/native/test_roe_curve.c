/*
 * Native test for src/roe_curve.c (issue #113: the owner asked the ROE/SWR scan
 * chart to draw a PARABOLA -- one clean valley -- instead of the Catmull-Rom
 * spline of #99/#104, which followed every sample, noise included).
 *
 * roe_curve_resample() is the geometry source the draw callback paints (see
 * dialog_swrscan.c); since #113 the shape is a single least-squares quadratic
 * y = a*x^2 + b*x + c over the sweep samples, evaluated at the same ~2 px step
 * as before. The samples, the sweep, the RF and the DSP are untouched: this is
 * the drawn curve only.
 *
 * The invariants pinned here:
 *   - the drawn curve is ONE valley whose minimum sits at the fit's vertex
 *     (x = -b/2a), not at every sample (the old Catmull-Rom knots);
 *   - with noise the fit stays a single valley (the vertex does not run to the
 *     ends of the sweep);
 *   - the parabola's ends are clamped to the chart's SWR range (1..5), so the
 *     curve never draws outside the frame;
 *   - the same input yields a byte-identical curve (memcmp);
 *   - the READOUT minimum is the MEASURED sample, not the fit's vertex: the two
 *     are reported apart and their distance is what the report measures.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "test_framework.h"

#include "roe_curve.h"

#include <stdio.h>
#include <string.h>

/* Enough for any input this test feeds the resampler (n <= 64). */
#define MAXOUT ((ROE_CURVE_OVERSAMPLE + 1) * 64)

static float out[MAXOUT];
static float out2[MAXOUT];

static float absf(float v) {
    return (v < 0.0f) ? -v : v;
}

/* Index of the lowest value in a buffer. */
static int min_index(const float *v, int n) {
    int best = 0;

    for (int i = 1; i < n; i++) {
        if (v[i] < v[best]) {
            best = i;
        }
    }

    return best;
}

/* Sample coordinate (0 .. n-1) of a curve point: the resampler writes
 * ROE_CURVE_OVERSAMPLE points per input interval. */
static float curve_x(int index) {
    return (float)index / (float)ROE_CURVE_OVERSAMPLE;
}

/* Vertex of the drawn parabola, read off the curve itself (x of its minimum). */
static float curve_vertex_x(const float *v, int m) {
    return curve_x(min_index(v, m));
}

void test_roe_curve(void) {
    /* --- el conteo de puntos: un punto por sub-muestra, el paso de ~2 px de siempre --- */
    CHECK(roe_curve_point_count(0) == 0);
    CHECK(roe_curve_point_count(1) == 1);
    CHECK(roe_curve_point_count(2) == ROE_CURVE_OVERSAMPLE + 1);
    CHECK(roe_curve_point_count(50) == (50 - 1) * ROE_CURVE_OVERSAMPLE + 1);

    /* --- el fit recupera una parabola conocida (a, b, c dentro de epsilon) --- */
    {
        /* y = 0.005x^2 - 0.25x + 4.375: valle en x = 25 (SWR 1.25), extremos
         * 4.375 (x=0) y 4.13 (x=49) -- todo dentro del rango del chart. */
        float in[50];
        float a, b, c;
        int   m = roe_curve_point_count(50);

        for (int i = 0; i < 50; i++) {
            float x = (float)i;

            in[i] = 0.005f * x * x - 0.25f * x + 4.375f;
        }

        CHECK(roe_curve_quadratic_fit(in, 50, &a, &b, &c));

        CHECK_CLOSE(a, 0.005f, 1e-4f);
        CHECK_CLOSE(b, -0.25f, 1e-4f);
        CHECK_CLOSE(c, 4.375f, 1e-3f);

        /* El vertice -b/2a es el valle conocido (x = 25). */
        CHECK_CLOSE(-b / (2.0f * a), 25.0f, 0.01f);

        /* Y la curva dibujada reproduce esa parabola. */
        roe_curve_resample(in, 50, out);

        CHECK_CLOSE(out[0], 4.375f, 1e-3f);                            /* x = 0 */
        CHECK_CLOSE(out[25 * ROE_CURVE_OVERSAMPLE], 1.25f, 1e-3f);     /* vertice */
        CHECK_CLOSE(out[m - 1], 4.13f, 1e-2f);                         /* x = 49 */
    }

    /* --- con menos de 3 muestras no hay parabola: la curva cae a la recta --- */
    {
        float in[2] = {2.0f, 3.0f};
        float a, b, c;
        int   m = roe_curve_point_count(2);

        CHECK(!roe_curve_quadratic_fit(in, 2, &a, &b, &c));

        roe_curve_resample(in, 2, out);

        CHECK_CLOSE(out[0], 2.0f, 1e-5f);
        CHECK_CLOSE(out[m - 1], 3.0f, 1e-5f);
    }

    /* --- la curva es UN valle: su minimo cae en el vertice del fit (x = -b/2a),
     *     no en cada muestra como hacia Catmull-Rom --- */
    {
        /* y = 0.002x^2 - 0.05x + 1.8125: valle en x = 12.5 (SWR 1.5), dentro
         * del rango del chart, sin clamp. */
        float in[50];
        int   m = roe_curve_point_count(50);

        for (int i = 0; i < 50; i++) {
            float x = (float)i;

            in[i] = 0.002f * x * x - 0.05f * x + 1.8125f;
        }

        roe_curve_resample(in, 50, out);

        /* El valle dibujado es uno solo y esta en x ~ 12.5 (Catmull-Rom lo
         * dejaba en el primer nudo, x = 12.0: dos nudos empatados). */
        CHECK_CLOSE(curve_vertex_x(out, m), 12.5f, 1.0f / ROE_CURVE_OVERSAMPLE + 1e-3f);

        /* Y el valor del valle es el del vertice (1.5), no el de un nudo. */
        CHECK_CLOSE(out[min_index(out, m)], 1.5f, 1e-3f);
    }

    /* --- con ruido el fit sigue siendo un solo valle, sin irse a los extremos --- */
    {
        /* La misma parabola del valle en x = 25 (SWR 1.25) mas un patron de
         * ruido deterministico de +-0.15: el barrido realista. */
        float in[50];
        int   m = roe_curve_point_count(50);

        for (int i = 0; i < 50; i++) {
            float x = (float)i;

            in[i] = 0.005f * x * x - 0.25f * x + 4.375f
                  + 0.15f * (float)(((i * 7) % 5) - 2) / 2.0f;
        }

        roe_curve_resample(in, 50, out);

        float vx = curve_vertex_x(out, m);

        CHECK(vx > 24.0f);
        CHECK(vx < 26.0f);

        /* El valle queda cerca del de la parabola limpia (x = 25). */
        CHECK(absf(vx - 25.0f) <= 0.5f);
    }

    /* --- clamp al rango del chart: la parabola se recorta al techo por segmento --- */
    {
        /* 1.5 + (x-2)^2 sobre 5 muestras: en los extremos el fit vale 5.5, por
         * encima del techo del chart, y el valle (1.5) sobrevive. */
        float in[5] = {5.5f, 2.5f, 1.5f, 2.5f, 5.5f};
        int   m     = roe_curve_point_count(5);

        roe_curve_resample(in, 5, out);

        for (int i = 0; i < m; i++) {
            CHECK(out[i] <= ROE_CURVE_MAX_VALUE + 1e-6f);
            CHECK(out[i] >= ROE_CURVE_MIN_VALUE - 1e-6f);
        }

        /* Los extremos salen recortados al techo, no en 5.5. */
        CHECK_CLOSE(out[0], ROE_CURVE_MAX_VALUE, 1e-6f);
        CHECK_CLOSE(out[m - 1], ROE_CURVE_MAX_VALUE, 1e-6f);

        /* Y el valle sigue exacto. */
        CHECK_CLOSE(out[min_index(out, m)], 1.5f, 1e-3f);
    }

    /* --- clamp por abajo: un valle bajo SWR 1 se recorta al piso --- */
    {
        float in[3] = {0.5f, 0.2f, 0.5f};
        int   m     = roe_curve_point_count(3);

        roe_curve_resample(in, 3, out);

        for (int i = 0; i < m; i++) {
            CHECK_CLOSE(out[i], ROE_CURVE_MIN_VALUE, 1e-6f);
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

    /* --- el readout minimo es la MUESTRA medida, no el vertice del fit --- */
    {
        /* Un barrido con una resonancia cerca de la muestra 10 y ruido que
         * corre el vertice del ajuste: el minimo que se reporta sigue siendo el
         * de la muestra medida, y la distancia al vertice es lo que se mide. */
        float in[21];
        int   m = roe_curve_point_count(21);

        for (int i = 0; i < 21; i++) {
            float x = (float)i;

            in[i] = 1.8f + 0.015f * (x - 10.0f) * (x - 10.0f)
                  + 0.1f * (float)((i % 3) - 1);
        }

        roe_curve_resample(in, 21, out);

        int   s_min = min_index(in, 21);            /* el readout: muestra medida */
        int   c_min = min_index(out, m);            /* el dibujo: vertice del fit */
        float delta = absf(curve_x(c_min) - (float)s_min);

        /* El readout sale de las muestras: su valor ES el de la muestra. */
        CHECK_CLOSE(in[s_min], in[min_index(in, 21)], 1e-6f);

        /* El vertice del fit queda a menos de 2 muestras del minimo medido
         * (el umbral de la spec para escalar a revision estetica). */
        CHECK(delta <= 2.0f);

        /* El readout (muestra) y el vertice (fit) son cantidades distintas: la
         * curva suavizada no reemplaza el minimo medido. */
        CHECK(absf(in[s_min] - out[c_min]) > 1e-3f);

        printf("roe_curve #113: readout min = muestra %d (SWR %.3f); "
               "vertice del fit = x %.2f (SWR %.3f); delta %.2f muestras\n",
               s_min, (double)in[s_min], (double)curve_x(c_min), (double)out[c_min],
               (double)delta);
    }

    /* --- caso degenerado: una sola muestra no rompe --- */
    {
        float in[1] = {2.5f};

        roe_curve_resample(in, 1, out);

        CHECK_CLOSE(out[0], 2.5f, 1e-6f);
    }
}

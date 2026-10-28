/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  ROE/SWR scan chart: fit the sweep samples to a parabola (issue #113) and
 *  resample it into the drawn curve. Render-only: the samples, the sweep, the RF
 *  and the DSP are untouched.
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "roe_curve.h"

#include <math.h>

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }

    return v;
}

/* Least-squares quadratic y = a*x^2 + b*x + c over n >= 3 samples, x = 0..n-1.
 *
 * The normal equations form a 3x3 symmetric system:
 *
 *   | S0 S1 S2 | | c |   | T0 |
 *   | S1 S2 S3 | | b | = | T1 |      Sk = sum(x^k), Tk = sum(y*x^k)
 *   | S2 S3 S4 | | a |   | T2 |
 *
 * solved by Cramer's rule. With n <= 51 and x <= 50 (the sweep's fixed grid, issue
 * #125) the sums stay tiny (S4 ~ 6.6e7, and every sum is an integer well below
 * 2^53) and the arithmetic is +,-,*,/ only, so no math.h is needed and the result
 * is deterministic. */
static bool quadratic_fit(const float *in, int n, double *a, double *b, double *c) {
    if (n < 3) {
        return false;
    }

    double s0 = 0.0, s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0;
    double t0 = 0.0, t1 = 0.0, t2 = 0.0;

    for (int i = 0; i < n; i++) {
        double x  = (double)i;
        double x2 = x * x;
        double y  = (double)in[i];

        s0 += 1.0;
        s1 += x;
        s2 += x2;
        s3 += x2 * x;
        s4 += x2 * x2;
        t0 += y;
        t1 += x * y;
        t2 += x2 * y;
    }

    double det = s0 * (s2 * s4 - s3 * s3)
               - s1 * (s1 * s4 - s3 * s2)
               + s2 * (s1 * s3 - s2 * s2);

    /* n >= 3 with x = 0..n-1 makes the system non-singular; the guard keeps the
     * division honest if that ever stops holding. */
    if (det == 0.0) {
        return false;
    }

    double det_c = t0 * (s2 * s4 - s3 * s3)
                 - s1 * (t1 * s4 - s3 * t2)
                 + s2 * (t1 * s3 - s2 * t2);

    double det_b = t1 * (s0 * s4 - s2 * s2)
                 - t0 * (s1 * s4 - s3 * s2)
                 - t2 * (s0 * s3 - s1 * s2);

    double det_a = t0 * (s1 * s3 - s2 * s2)
                 - t1 * (s0 * s3 - s1 * s2)
                 + t2 * (s0 * s2 - s1 * s1);

    *a = det_a / det;
    *b = det_b / det;
    *c = det_c / det;

    return true;
}

/* Least-squares straight line y = m*x + k through n >= 1 samples; the fallback
 * for n == 2, where a parabola is not determined. Exact for two points. */
static void line_fit(const float *in, int n, double *m, double *k) {
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;

    for (int i = 0; i < n; i++) {
        double x = (double)i;
        double y = (double)in[i];

        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }

    double den = (double)n * sxx - sx * sx;

    *m = (den == 0.0) ? 0.0 : ((double)n * sxy - sx * sy) / den;
    *k = (sy - *m * sx) / (double)n;
}

bool roe_curve_quadratic_fit(const float *in, int n, float *a, float *b, float *c) {
    double da = 0.0, db = 0.0, dc = 0.0;

    if (!quadratic_fit(in, n, &da, &db, &dc)) {
        return false;
    }

    *a = (float)da;
    *b = (float)db;
    *c = (float)dc;

    return true;
}

int roe_curve_point_count(int n) {
    if (n <= 0) {
        return 0;
    }
    if (n == 1) {
        return 1;
    }

    return (n - 1) * ROE_CURVE_OVERSAMPLE + 1;
}

/* Monotone cubic Hermite (Fritsch-Carlson) through the samples, sampled at the
 * fixed ROE_CURVE_OVERSAMPLE step. Passes through every input point (t=0 of each
 * interval is the sample) and never overshoots a monotone run. */
void roe_curve_interpolate(const float *in, int n, float *out) {
    if (n <= 0) {
        return;
    }

    if (n == 1) {
        out[0] = clampf(in[0], ROE_CURVE_MIN_VALUE, ROE_CURVE_MAX_VALUE);
        return;
    }

    double d[ROE_CURVE_MAX_POINTS];
    double m[ROE_CURVE_MAX_POINTS];

    for (int i = 0; i < n - 1; i++) {
        d[i] = (double) in[i + 1] - (double) in[i];
    }

    /* Tangents: harmonic mean of the neighbouring slopes, zero at sign changes. */
    m[0]     = d[0];
    m[n - 1] = d[n - 2];

    for (int i = 1; i < n - 1; i++) {
        if (d[i - 1] * d[i] <= 0.0) {
            m[i] = 0.0;
        } else {
            m[i] = 2.0 * d[i - 1] * d[i] / (d[i - 1] + d[i]);
        }
    }

    /* Fritsch-Carlson limiter: keeps the cubic monotone in each interval. */
    for (int i = 0; i < n - 1; i++) {
        if (d[i] == 0.0) {
            m[i]     = 0.0;
            m[i + 1] = 0.0;
            continue;
        }

        double a = m[i] / d[i];
        double b = m[i + 1] / d[i];
        double s = a * a + b * b;

        if (s > 9.0) {
            double tau = 3.0 / sqrt(s);

            m[i]     = tau * a * d[i];
            m[i + 1] = tau * b * d[i];
        }
    }

    for (int i = 0; i < n - 1; i++) {
        for (int k = 0; k < ROE_CURVE_OVERSAMPLE; k++) {
            double t = (double) k / (double) ROE_CURVE_OVERSAMPLE;
            double t2 = t * t;
            double t3 = t2 * t;
            double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
            double h10 = t3 - 2.0 * t2 + t;
            double h01 = -2.0 * t3 + 3.0 * t2;
            double h11 = t3 - t2;
            double y = h00 * in[i] + h10 * m[i] + h01 * in[i + 1] + h11 * m[i + 1];

            out[i * ROE_CURVE_OVERSAMPLE + k] =
                clampf((float) y, ROE_CURVE_MIN_VALUE, ROE_CURVE_MAX_VALUE);
        }
    }

    out[(n - 1) * ROE_CURVE_OVERSAMPLE] =
        clampf(in[n - 1], ROE_CURVE_MIN_VALUE, ROE_CURVE_MAX_VALUE);
}

void roe_curve_resample(const float *in, int n, float *out) {
    if (n <= 0) {
        return;
    }

    if (n == 1) {
        out[0] = clampf(in[0], ROE_CURVE_MIN_VALUE, ROE_CURVE_MAX_VALUE);
        return;
    }

    /* The drawn shape: the least-squares parabola of the samples, or the
     * straight line through them when a parabola is not determined (n == 2). */
    double a = 0.0, b = 0.0, c = 0.0;

    if (!quadratic_fit(in, n, &a, &b, &c)) {
        line_fit(in, n, &b, &c);
    }

    /* A ROE sweep over an antenna resonance is physically a VALLEY: the measured
     * curve must open upward (base at the bottom, both tips rising). The free
     * least-squares fit does not enforce that, so a measured run that is not a
     * clean valley can come out inverted -- a dome, a < 0 -- and the chart used
     * to draw it upside down (the owner's report: "la parabola esta invertida:
     * base abajo y las puntas a infinito positivo"). When the fit opens
     * downward, mirror it into the valley that has the same vertex column and
     * its base on the measured minimum: y = ymin + |a|*(x - xv)^2. The vertex is
     * clamped inside [0, n-1] so an almost-flat fit (whose vertex runs off the
     * run) becomes a shallow valley with its base on the nearest edge instead of
     * a curve pinned to the chart ceiling. A fit that already opens upward is
     * drawn as-is: the measured minimum of the readout never comes from here. */
    if (a < 0.0) {
        double xv = -b / (2.0 * a);
        double ymin = (double) in[0];

        if (xv < 0.0) {
            xv = 0.0;
        } else if (xv > (double) (n - 1)) {
            xv = (double) (n - 1);
        }

        for (int i = 1; i < n; i++) {
            if ((double) in[i] < ymin) {
                ymin = (double) in[i];
            }
        }

        a = -a;
        b = -2.0 * a * xv;
        c = ymin + a * xv * xv;
    }

    int m = roe_curve_point_count(n);

    for (int i = 0; i < m; i++) {
        double x = (double)i / (double)ROE_CURVE_OVERSAMPLE;
        double y = a * x * x + b * x + c;

        /* The parabola runs to infinity at the ends: clip each point to the
         * chart's SWR range so the curve never draws outside the frame. */
        out[i] = clampf((float)y, ROE_CURVE_MIN_VALUE, ROE_CURVE_MAX_VALUE);
    }
}

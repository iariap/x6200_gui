/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  ROE/SWR scan chart: resample the sweep samples into a smooth curve
 *  (issue #99). Render-only: the samples, the sweep and the DSP are untouched.
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "roe_curve.h"

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }

    return v;
}

/* Uniform Catmull-Rom over four control points, at parameter t in [0,1]. */
static float catmull_rom(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5f * ((2.0f * p1)
                 + (-p0 + p2) * t
                 + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
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

void roe_curve_resample(const float *in, int n, float *out) {
    if (n <= 0) {
        return;
    }

    if (n == 1) {
        out[0] = clampf(in[0], ROE_CURVE_MIN_VALUE, ROE_CURVE_MAX_VALUE);
        return;
    }

    int m = roe_curve_point_count(n);

    for (int i = 0; i < m; i++) {
        int seg = i / ROE_CURVE_OVERSAMPLE;

        /* The last point (t == 1) belongs to the last interval. */
        if (seg > n - 2) {
            seg = n - 2;
        }

        int   rem = i - seg * ROE_CURVE_OVERSAMPLE;
        float t   = (float)rem / (float)ROE_CURVE_OVERSAMPLE;

        float p1 = in[seg];
        float p2 = in[seg + 1];
        float p0 = in[(seg > 0) ? seg - 1 : 0];
        float p3 = in[(seg + 2 < n) ? seg + 2 : n - 1];

        float v = catmull_rom(p0, p1, p2, p3, t);

        /* No overshoot: the curve stays between the two samples that bracket the
         * point, so a resonance (a local minimum) is never deepened and a peak is
         * never amplified by the smoothing. */
        float lo = (p1 < p2) ? p1 : p2;
        float hi = (p1 < p2) ? p2 : p1;

        v = clampf(v, lo, hi);

        out[i] = clampf(v, ROE_CURVE_MIN_VALUE, ROE_CURVE_MAX_VALUE);
    }
}

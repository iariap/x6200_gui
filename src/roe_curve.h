/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  ROE/SWR scan chart: fit the sweep samples to a parabola and resample that
 *  single valley into the points the chart draws.
 *
 *  Render-only helper for dialog_swrscan.c. #99 drew a Catmull-Rom spline that
 *  passed through every sample (noise included) and read as a jagged curve; #113
 *  (owner's order) replaced it with one clean valley: a least-squares quadratic
 *  over all the samples. The samples themselves are never touched -- the sweep,
 *  the RF and the DSP are out of scope here.
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>

/* The chart's y axis spans SWR 1..5 (see calc_y() in dialog_swrscan.c: SWR 1
 * sits at the bottom, SWR 5 at the top). The parabola runs to infinity at the
 * ends, so each resampled value is clamped to that range and the curve never
 * draws outside the frame; SWR is physically >= 1. */
#define ROE_CURVE_MIN_VALUE 1.0f
#define ROE_CURVE_MAX_VALUE 5.0f

/* Sub-points per input interval. The chart is 780 px wide with 50 samples
 * (~15 px apart), so 8 sub-points land one segment every ~2 px: smooth on the
 * panel without depending on the pixel width (deterministic). */
#define ROE_CURVE_OVERSAMPLE 8

/* Number of points roe_curve_resample() writes for `n` input samples. */
int roe_curve_point_count(int n);

/* Least-squares quadratic fit of the sweep samples: y = a*x^2 + b*x + c, with x
 * in sample units (0 .. n-1). This is the shape roe_curve_resample() draws.
 *
 * Ordinary least squares over ALL the samples (not a robust fit): it is the
 * stable, closed-form choice for a noisy sweep, and every sample weighs the
 * same. Only +,-,*,/ are used, so this file stays free of math.h (it compiles in
 * the native stage, the firmware and the UI harness as they are today).
 *
 * Returns false when n < 3 (no parabola: roe_curve_resample() then draws the
 * straight line through the samples) and leaves a, b, c untouched.
 *
 * Deterministic: the same input always yields the same coefficients. */
bool roe_curve_quadratic_fit(const float *in, int n, float *a, float *b, float *c);

/* Resample `n` samples of `in` into `out`, which must hold at least
 * roe_curve_point_count(n) floats.
 *
 * The curve is the least-squares parabola of the samples, evaluated at a fixed
 * step (~2 px on the panel): one clean valley instead of a spline that follows
 * every sample. Each point is clamped to [ROE_CURVE_MIN_VALUE,
 * ROE_CURVE_MAX_VALUE], which clips the parabola's ends to the chart's ceiling.
 * The measured minimum (the resonance the operator is after) is read off the
 * samples, never off this curve -- the fit is the drawn shape only.
 *
 * Deterministic: the same input always yields the same output. */
void roe_curve_resample(const float *in, int n, float *out);

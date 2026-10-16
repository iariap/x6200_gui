/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  ROE/SWR scan chart: resample the sweep samples into a smooth curve.
 *
 *  Render-only helper for dialog_swrscan.c (issue #99): the chart used to join
 *  the raw samples with straight segments, which reads as a sawtooth and is not
 *  representative of the antenna. The samples themselves are never touched --
 *  the sweep, the RF and the DSP are out of scope here.
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>

/* The chart's y axis spans SWR 1..5 (see calc_y() in dialog_swrscan.c: SWR 1
 * sits at the bottom, SWR 5 at the top). A resampled value is clamped to that
 * range so the smoothing never draws outside the frame; SWR is physically >= 1. */
#define ROE_CURVE_MIN_VALUE 1.0f
#define ROE_CURVE_MAX_VALUE 5.0f

/* Sub-points per input interval. The chart is 780 px wide with 50 samples
 * (~15 px apart), so 8 sub-points land one segment every ~2 px: smooth on the
 * panel without depending on the pixel width (deterministic). */
#define ROE_CURVE_OVERSAMPLE 8

/* Number of points roe_curve_resample() writes for `n` input samples. */
int roe_curve_point_count(int n);

/* Resample `n` samples of `in` into `out`, which must hold at least
 * roe_curve_point_count(n) floats.
 *
 * The curve is a uniform Catmull-Rom cubic through the samples, evaluated at a
 * fixed step, so every sample stays an exact knot of the curve: the first and
 * last samples and the local minima (the antenna resonances) are preserved.
 * Each interpolated point is clamped to the value range of the two samples that
 * bracket it, which removes Catmull-Rom's overshoot: the curve never invents a
 * dip below a resonance nor a peak above a sample. Values are additionally
 * clamped to [ROE_CURVE_MIN_VALUE, ROE_CURVE_MAX_VALUE].
 *
 * Deterministic: the same input always yields the same output. */
void roe_curve_resample(const float *in, int n, float *out);

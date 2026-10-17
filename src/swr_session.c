/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  SWR scan session engine (issue #125). See swr_session.h for the contract.
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "swr_session.h"

#include <math.h>
#include <string.h>

void swr_limits_default(swr_limits_t *out) {
    if (!out) {
        return;
    }

    out->max_passes          = SWR_SESSION_PASSES_QUICK;
    out->settle_ms           = 0;
    out->alternate_direction = false;
    out->pass_timeout_ms     = 0;   /* caller decides (radio-flow constant) */
    out->session_timeout_ms  = 0;
}

bool swr_sample_valid(float swr) {
    if (!isfinite(swr)) {
        return false;
    }

    return swr >= SWR_SESSION_SWR_MIN && swr <= SWR_SESSION_SWR_MAX;
}

/* freq[i] = start + round(span * i / (points - 1)), all in 64 bits. */
static int64_t grid_freq_at(int64_t start, int64_t span, int64_t denom, int i) {
    int64_t num = span * (int64_t) i;
    int64_t q   = num / denom;
    int64_t r   = num % denom;

    /* round half up; r >= 0 because span > 0 and i >= 0 */
    if (r * 2 >= denom) {
        q++;
    }

    return start + q;
}

swr_grid_result_t swr_grid_build(uint32_t center_hz, int32_t span_hz, int points, swr_grid_t *out) {
    if (!out || points < 2 || points > SWR_SESSION_POINTS || span_hz <= 0) {
        return SWR_GRID_BAD_PARAMS;
    }

    int64_t half  = (int64_t) span_hz / 2;
    int64_t start = (int64_t) center_hz - half;
    int64_t stop  = start + (int64_t) span_hz;

    if (start < 0 || stop > (int64_t) UINT32_MAX) {
        return SWR_GRID_OVERFLOW;
    }

    int64_t denom = (int64_t) points - 1;

    for (int i = 0; i < points; i++) {
        int64_t f = grid_freq_at(start, span_hz, denom, i);

        if (f < (int64_t) SWR_SESSION_TX_MIN_HZ || f > (int64_t) SWR_SESSION_TX_MAX_HZ) {
            return SWR_GRID_RANGE;
        }

        out->freq_hz[i] = (uint32_t) f;
    }

    /* Monotone and unique: a span narrower than points-1 Hz cannot hold the
     * grid without repeating a frequency, so it is refused instead of sweeping
     * the same point twice. */
    for (int i = 1; i < points; i++) {
        if (out->freq_hz[i] <= out->freq_hz[i - 1]) {
            return SWR_GRID_BAD_PARAMS;
        }
    }

    out->points    = points;
    out->center_hz = center_hz;
    out->span_hz   = span_hz;
    out->step_hz   = (uint32_t) ((int64_t) span_hz / denom);

    return SWR_GRID_OK;
}

static void sort_floats(float *v, int n) {
    for (int i = 1; i < n; i++) {
        float key = v[i];
        int   j   = i - 1;

        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            j--;
        }

        v[j + 1] = key;
    }
}

void swr_point_stat(const swr_point_t *p, swr_point_stat_t *out) {
    if (!p || !out) {
        return;
    }

    out->median    = NAN;
    out->mad       = NAN;
    out->n_valid   = 0;
    out->n_invalid = p->n_invalid;
    out->saturated = p->saturated;

    float v[SWR_SESSION_MAX_PASSES];
    int   n = 0;

    for (int i = 0; i < SWR_SESSION_MAX_PASSES; i++) {
        if (p->samples[i].valid) {
            v[n++] = p->samples[i].swr;
        }
    }

    out->n_valid = n;

    if (n == 0) {
        return;
    }

    sort_floats(v, n);

    out->median = (n % 2) ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);

    if (n < 2) {
        return;
    }

    float dev[SWR_SESSION_MAX_PASSES];

    for (int i = 0; i < n; i++) {
        dev[i] = fabsf(v[i] - out->median);
    }

    sort_floats(dev, n);

    out->mad = (n % 2) ? dev[n / 2] : 0.5f * (dev[n / 2 - 1] + dev[n / 2]);
}

bool swr_session_point_stat(const swr_session_t *s, int point, swr_point_stat_t *out) {
    if (!s || !out || point < 0 || point >= s->grid.points) {
        return false;
    }

    swr_point_stat(&s->points[point], out);

    return true;
}

int swr_session_valid_points(const swr_session_t *s) {
    if (!s) {
        return 0;
    }

    int n = 0;

    for (int i = 0; i < s->grid.points; i++) {
        if (s->points[i].n_valid > 0) {
            n++;
        }
    }

    return n;
}

bool swr_session_minimum(const swr_session_t *s, int *out_point, float *out_swr, float *out_mad) {
    if (!s) {
        return false;
    }

    bool  found = false;
    float best  = 0.0f;
    int   best_i = -1;
    float best_mad = NAN;

    for (int i = 0; i < s->grid.points; i++) {
        swr_point_stat_t st;

        swr_point_stat(&s->points[i], &st);

        if (st.n_valid == 0 || isnan(st.median)) {
            continue;
        }

        if (!found || st.median < best) {
            found    = true;
            best     = st.median;
            best_i   = i;
            best_mad = st.mad;
        }
    }

    if (!found) {
        return false;
    }

    if (out_point) {
        *out_point = best_i;
    }
    if (out_swr) {
        *out_swr = best;
    }
    if (out_mad) {
        *out_mad = best_mad;
    }

    return true;
}

uint32_t swr_session_freq(const swr_session_t *s, int point) {
    if (!s || point < 0 || point >= s->grid.points) {
        return 0;
    }

    return s->points[point].freq_hz;
}

/* The storage index a pass starts at: 0 going up, the top of the grid going down
 * (odd passes when alternate_direction is on). The storage index IS the grid
 * index, so a sample is always recorded next to its own frequency. */
static int pass_first_index(const swr_session_t *s, int pass) {
    return (s->limits.alternate_direction && (pass % 2) == 1) ? s->grid.points - 1 : 0;
}

static int pass_step(const swr_session_t *s, int pass) {
    return (s->limits.alternate_direction && (pass % 2) == 1) ? -1 : 1;
}

static bool index_in_pass(const swr_session_t *s, int pass, int point) {
    return (pass_step(s, pass) > 0) ? (point < s->grid.points) : (point >= 0);
}

void swr_session_abort(swr_session_t *s, swr_abort_reason_t reason, uint32_t now_ms) {
    if (!s || (s->state != SWR_SESSION_IDLE && s->state != SWR_SESSION_SCANNING)) {
        return;
    }

    s->state        = (reason == SWR_ABORT_USER) ? SWR_SESSION_STOPPED : SWR_SESSION_ERROR;
    s->abort_reason = reason;
    s->finished_ms  = now_ms;
}

swr_action_t swr_session_begin(swr_session_t *s, const swr_grid_t *grid, int passes_requested,
                               const swr_limits_t *limits, uint32_t now_ms, uint32_t *out_freq) {
    if (!s || !grid || !limits) {
        return SWR_ACT_ABORT;
    }

    memset(s, 0, sizeof(*s));

    s->grid             = *grid;
    s->limits           = *limits;
    s->started_ms       = now_ms;
    s->pass_started_ms  = now_ms;
    s->passes_requested = passes_requested;
    s->state            = SWR_SESSION_IDLE;
    s->abort_reason     = SWR_ABORT_NONE;

    for (int i = 0; i < grid->points; i++) {
        s->points[i].freq_hz = grid->freq_hz[i];
    }

    if (passes_requested < 1) {
        s->state        = SWR_SESSION_ERROR;
        s->abort_reason = SWR_ABORT_INVALID;
        return SWR_ACT_ABORT;
    }

    if (passes_requested > SWR_SESSION_MAX_PASSES || passes_requested > limits->max_passes) {
        s->state        = SWR_SESSION_ERROR;
        s->abort_reason = SWR_ABORT_MULTIPASS_GATED;
        return SWR_ACT_ABORT;
    }

    s->state            = SWR_SESSION_SCANNING;
    s->pass             = 0;
    s->point            = pass_first_index(s, 0);
    s->point_seq        = 0;
    s->tuned_ms         = now_ms;

    if (out_freq) {
        *out_freq = swr_session_freq(s, s->point);
    }

    return SWR_ACT_SET_FREQ;
}

static void record_sample(swr_session_t *s, float swr, uint32_t now_ms) {
    swr_point_t  *p = &s->points[s->point];
    swr_sample_t *sample = &p->samples[s->pass];

    sample->swr          = swr;
    sample->timestamp_ms = now_ms;
    sample->valid        = swr_sample_valid(swr);

    if (sample->valid) {
        p->n_valid++;

        if (swr >= SWR_SESSION_SWR_SATURATION) {
            p->saturated = true;
        }
    } else {
        p->n_invalid++;
    }
}

swr_action_t swr_session_packet(swr_session_t *s, float swr, uint32_t now_ms, uint32_t *out_freq) {
    if (!s || s->state != SWR_SESSION_SCANNING) {
        return SWR_ACT_NONE;
    }

    s->packets++;

    /* The settle window drops the packets that may still carry the previous
     * frequency: the flow gives no confirmation of when the VFO actually moved,
     * so the caller tells us when it commanded it (swr_session_tuned) and any
     * packet younger than limits.settle_ms after that is stale. A stale packet
     * is NOT stored and does NOT advance the sweep (the premature advance would
     * attribute the next frequency's real reading to this point). */
    if (s->limits.settle_ms > 0 && (uint32_t) (now_ms - s->tuned_ms) < s->limits.settle_ms) {
        s->discarded++;
        return SWR_ACT_NONE;
    }

    record_sample(s, swr, now_ms);

    s->point     += pass_step(s, s->pass);
    s->point_seq += 1;

    if (index_in_pass(s, s->pass, s->point)) {
        s->tuned_ms = now_ms;

        if (out_freq) {
            *out_freq = swr_session_freq(s, s->point);
        }

        return SWR_ACT_SET_FREQ;
    }

    s->passes_completed++;

    if (s->pass + 1 < s->passes_requested) {
        s->pass++;
        s->point           = pass_first_index(s, s->pass);
        s->point_seq       = 0;
        s->tuned_ms        = now_ms;
        s->pass_started_ms = now_ms;

        if (out_freq) {
            *out_freq = swr_session_freq(s, s->point);
        }

        return SWR_ACT_SET_FREQ;
    }

    s->state       = SWR_SESSION_COMPLETED;
    s->finished_ms = now_ms;

    return SWR_ACT_COMPLETE;
}

void swr_session_tuned(swr_session_t *s, uint32_t now_ms) {
    if (!s || s->state != SWR_SESSION_SCANNING) {
        return;
    }

    s->tuned_ms = now_ms;
}

swr_action_t swr_session_poll(swr_session_t *s, uint32_t now_ms) {
    if (!s || s->state != SWR_SESSION_SCANNING) {
        return SWR_ACT_NONE;
    }

    bool pass_stalled    = s->limits.pass_timeout_ms &&
                           (uint32_t) (now_ms - s->pass_started_ms) > s->limits.pass_timeout_ms;
    bool session_stalled = s->limits.session_timeout_ms &&
                           (uint32_t) (now_ms - s->started_ms) > s->limits.session_timeout_ms;

    if (pass_stalled || session_stalled) {
        swr_session_abort(s, SWR_ABORT_TIMEOUT, now_ms);
        return SWR_ACT_ABORT;
    }

    return SWR_ACT_NONE;
}

swr_action_t swr_session_stop(swr_session_t *s, uint32_t now_ms) {
    if (!s || s->state != SWR_SESSION_SCANNING) {
        return SWR_ACT_NONE;
    }

    swr_session_abort(s, SWR_ABORT_USER, now_ms);

    return SWR_ACT_ABORT;
}

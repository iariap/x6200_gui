/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  SWR scan session engine (issue #125): the measurement half of the SWR Scan,
 *  separated from LVGL and from the radio. It owns the frequency grid, the
 *  per-point samples of every pass, the robust aggregation (median, MAD) and
 *  the measured minimum. It never touches LVGL, never touches the radio: the
 *  caller performs the radio action the engine returns (set the VFO, stop TX).
 *
 *  Why a session and not the old per-packet step: the old sweep stored the
 *  packet it got right after `radio_set_freq()` as the sample of the requested
 *  frequency (one packet == one point, no confirmation) and drew a 5-point
 *  moving average across NEIGHBOURING frequencies -- which mixes different
 *  frequencies into one value (issue #125 forbids it). Here every sample is
 *  keyed by (pass, point) over one frozen grid, several passes hit exactly the
 *  same frequencies, and the curve comes from the per-frequency aggregate.
 *
 *  Honesty rules baked in:
 *   - a missing/invalid reading is NEVER turned into SWR 1.0 (NaN instead);
 *   - SWR < 1 is not a measurement, so it is invalid;
 *   - the minimum comes from the aggregated MEASURED data, never from the
 *     drawn quadratic fit (which is decoration, see dialog_swrscan.c);
 *   - the per-point spread (MAD) is reported so a number is not shown with
 *     more confidence than the 0.1 SWR detector resolution supports.
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 51 points per sweep, including both span endpoints, independent of the span
 * (issue #125). The span only changes the step: 50 kHz -> 1 kHz, 100 kHz ->
 * 2 kHz, 200 kHz -> 4 kHz, 500 kHz -> 10 kHz. */
#define SWR_SESSION_POINTS        51

/* Passes: 1 = Quick, 3 = Stable, 5 = Fine. The engine supports all of them;
 * whether the RADIO may transmit that many is a separate gate (see
 * SWRSCAN_MULTIPASS_TX_ENABLED in dialog_swrscan.h). */
#define SWR_SESSION_MAX_PASSES    5
#define SWR_SESSION_PASSES_QUICK  1
#define SWR_SESSION_PASSES_STABLE 3
#define SWR_SESSION_PASSES_FINE   5

/* Detector resolution: src/radio.c feeds `pack->vswr * 0.1f`. */
#define SWR_SESSION_QUANTUM       0.1f

/* Electrical validity window of ONE reading. SWR < 1 is not a measurement (the
 * X6200 detector reports >= 1.0); `pack->vswr` is a byte, so * 0.1 tops out at
 * 25.5. A reading at/above SWR_SESSION_SWR_SATURATION is kept (it is a real
 * "very high" reading) but the point is flagged saturated: the detector is at
 * its ceiling and the number is not trustworthy. */
#define SWR_SESSION_SWR_MIN        1.0f
#define SWR_SESSION_SWR_MAX        25.5f
#define SWR_SESSION_SWR_SATURATION 9.9f

/* Legal TX window of the X6200 (src/radio.c: radio_check_freq). A grid with any
 * point outside it is refused: the sweep never transmits out of band. */
#define SWR_SESSION_TX_MIN_HZ      500000u
#define SWR_SESSION_TX_MAX_HZ      55000000u

/* --- grid ----------------------------------------------------------------- */

typedef struct {
    uint32_t freq_hz[SWR_SESSION_POINTS];
    int      points;
    uint32_t center_hz;
    int32_t  span_hz;
    uint32_t step_hz;       /* nominal step (span / (points-1), truncated) */
} swr_grid_t;

typedef enum {
    SWR_GRID_OK = 0,
    SWR_GRID_BAD_PARAMS,    /* points < 2, span <= 0, or span too narrow to hold the points */
    SWR_GRID_OVERFLOW,      /* an endpoint is not representable in uint32 */
    SWR_GRID_RANGE,         /* a point falls outside the legal TX window */
} swr_grid_result_t;

/* Builds `points` frequencies over [center - span/2, center + span/2], both
 * endpoints included, with `freq[i] = start + round(span * i / (points-1))`
 * evaluated in 64 bits (no iterative sum, no accumulated error). Verifies the
 * endpoints, monotonicity, uniqueness, overflow and the legal TX window.
 * Deterministic: the same (center, span, points) always yields the same grid. */
swr_grid_result_t swr_grid_build(uint32_t center_hz, int32_t span_hz, int points, swr_grid_t *out);

/* --- samples and session --------------------------------------------------- */

typedef struct {
    float    swr;
    uint32_t timestamp_ms;
    bool     valid;
} swr_sample_t;

typedef struct {
    uint32_t     freq_hz;
    swr_sample_t samples[SWR_SESSION_MAX_PASSES];   /* indexed by pass */
    int          n_valid;
    int          n_invalid;
    bool         saturated;
} swr_point_t;

typedef enum {
    SWR_SESSION_IDLE = 0,
    SWR_SESSION_SCANNING,
    SWR_SESSION_COMPLETED,
    SWR_SESSION_STOPPED,
    SWR_SESSION_ERROR,
} swr_session_state_t;

typedef enum {
    SWR_ABORT_NONE = 0,
    SWR_ABORT_USER,             /* Stop / Escape / close */
    SWR_ABORT_TIMEOUT,          /* the pass/session watchdog fired */
    SWR_ABORT_NO_DATA,          /* no usable packet */
    SWR_ABORT_NO_TX,            /* the flow says the radio is not transmitting */
    SWR_ABORT_STATE_CHANGED,    /* the radio left RADIO_SWRSCAN */
    SWR_ABORT_MULTIPASS_GATED,  /* 3/5 passes need hardware validation */
    SWR_ABORT_RANGE,            /* the grid leaves the legal TX window */
    SWR_ABORT_INVALID,          /* bad parameters */
} swr_abort_reason_t;

/* What the caller has to do after each engine call. */
typedef enum {
    SWR_ACT_NONE = 0,   /* nothing to do */
    SWR_ACT_SET_FREQ,   /* radio_set_freq(*out_freq) */
    SWR_ACT_COMPLETE,   /* session finished: stop TX, freeze */
    SWR_ACT_ABORT,      /* session aborted: stop TX, keep the partial */
} swr_action_t;

typedef struct {
    int      max_passes;          /* TX gate: 1 unless the multipass was validated */
    uint32_t settle_ms;           /* packets younger than this after a set_freq are stale */
    bool     alternate_direction; /* odd passes sweep high -> low */
    uint32_t pass_timeout_ms;     /* stall watchdog of one pass */
    uint32_t session_timeout_ms;  /* stall watchdog of the whole session */
} swr_limits_t;

typedef struct {
    swr_grid_t          grid;
    swr_limits_t        limits;
    swr_session_state_t state;
    swr_abort_reason_t  abort_reason;
    int                 passes_requested;
    int                 pass;              /* current pass (0-based) */
    int                 point;             /* current grid/storage index (the sweep cursor) */
    int                 point_seq;         /* acquisition order inside the pass (0-based) */
    int                 passes_completed;
    uint32_t            tuned_ms;          /* when the VFO was commanded for this point */
    uint32_t            started_ms;
    uint32_t            pass_started_ms;
    uint32_t            finished_ms;
    uint64_t            packets;           /* packets seen while scanning */
    uint64_t            discarded;         /* packets dropped as stale */
    swr_point_t         points[SWR_SESSION_POINTS];
} swr_session_t;

typedef struct {
    float median;   /* NaN when the point has no valid sample */
    float mad;      /* NaN when fewer than 2 valid samples */
    int   n_valid;
    int   n_invalid;
    bool  saturated;
} swr_point_stat_t;

/* Safe defaults: 1 pass, no settle, no alternating direction. The caller sets
 * the budgets (they are radio-flow constants, see dialog_swrscan.h). */
void swr_limits_default(swr_limits_t *out);

/* A single reading is valid when it is finite and inside
 * [SWR_SESSION_SWR_MIN, SWR_SESSION_SWR_MAX]. Missing (0/NaN/Inf) and SWR < 1
 * are invalid; they are never replaced by 1.0. */
bool swr_sample_valid(float swr);

/* Starts a session over `grid`. Returns SWR_ACT_SET_FREQ and writes the first
 * frequency to `out_freq`, or SWR_ACT_ABORT when the request is invalid
 * (passes out of range -> SWR_ABORT_MULTIPASS_GATED / INVALID; the session is
 * left in SWR_SESSION_ERROR with the reason). */
swr_action_t swr_session_begin(swr_session_t *s, const swr_grid_t *grid, int passes_requested,
                               const swr_limits_t *limits, uint32_t now_ms, uint32_t *out_freq);

/* Feeds one packet of the radio flow. A packet that arrives inside the settle
 * window of the current point (limits.settle_ms after `swr_session_tuned`) is
 * discarded as stale: it may still carry the previous frequency. Otherwise it
 * is recorded for the current (pass, point) and the engine advances. Returns the
 * next radio action. */
swr_action_t swr_session_packet(swr_session_t *s, float swr, uint32_t now_ms, uint32_t *out_freq);

/* Records an INVALID sample for the current (pass, point) and advances exactly like
 * swr_session_packet(). Used when the flow delivers a packet without TX power: the point is
 * left as a gap (no reading) instead of being stored as ROE 1.0. */
swr_action_t swr_session_skip_point(swr_session_t *s, uint32_t now_ms, uint32_t *out_freq);

/* Tells the engine when the caller actually commanded the VFO the last
 * SWR_ACT_SET_FREQ asked for. The settle window is measured from here, not from
 * the instant the engine returned the action (the flow can take its own time to
 * move the VFO). `swr_session_begin` and every SET_FREQ return already set it to
 * their own `now_ms`, so a caller that never calls this still gets a sane
 * reference; calling it makes the reference the real command time. No-op unless
 * the session is scanning. */
void swr_session_tuned(swr_session_t *s, uint32_t now_ms);

/* Watchdog check for when the flow goes quiet (no packets): aborts on timeout.
 * Meant to be called from the radio thread, not from LVGL. */
swr_action_t swr_session_poll(swr_session_t *s, uint32_t now_ms);

/* Operator Stop. Idempotent: once the session is not scanning it is a no-op. */
swr_action_t swr_session_stop(swr_session_t *s, uint32_t now_ms);

/* Marks the session aborted with `reason` (USER -> STOPPED, the rest -> ERROR).
 * No-op when the session already ended. */
void swr_session_abort(swr_session_t *s, swr_abort_reason_t reason, uint32_t now_ms);

/* Median (robust default; a trimmed mean over <= 5 quantized samples would be
 * less robust) and MAD of the valid samples of a point. */
void swr_point_stat(const swr_point_t *p, swr_point_stat_t *out);

/* Same, for the point `point` of a session (false when out of range). */
bool swr_session_point_stat(const swr_session_t *s, int point, swr_point_stat_t *out);

/* Minimum of the AGGREGATED (median) valid data. Never from a drawn fit.
 * Returns false when no point has a valid sample; the outputs are untouched. */
bool swr_session_minimum(const swr_session_t *s, int *out_point, float *out_swr, float *out_mad);

/* How many of the grid points have at least one valid sample. */
int swr_session_valid_points(const swr_session_t *s);

/* Frequency the session stores at grid index `point`. The storage index is the
 * grid index, so a sample always sits next to its own frequency even on the
 * descending (alternate-direction) passes. */
uint32_t swr_session_freq(const swr_session_t *s, int point);

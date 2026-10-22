/*
 * Native tests for the SWR scan session engine (src/swr_session.c, issue #125).
 *
 * The engine is the measurement half of the SWR Scan: the 51-point grid, the
 * per-point samples of every pass, the robust aggregation (median/MAD) and the
 * measured minimum. It is pure C -- no LVGL, no radio -- so it is tested here
 * directly, and the dialog drives the very same functions (see
 * test_swrscan.c and test_swrscan_multipass.c).
 *
 * The invariants this file pins (issue #125):
 *   - 50/100/200/500 kHz all give exactly 51 frequencies, both endpoints, with
 *     the adaptive step (1/2/4/10 kHz) and a bounded rounding on non-divisible
 *     spans; overflow and out-of-band grids are refused, never swept;
 *   - several passes hit EXACTLY the same frequencies per index (no mixing) and
 *     each index aggregates its own samples (median), never neighbouring ones;
 *   - an outlier does not create a false valley; a low spike does not create a
 *     false minimum either;
 *   - a missing/NaN/Inf/<1 reading is NOT turned into SWR 1.0;
 *   - the minimum comes from the aggregated measured data, not from any fit;
 *   - stop is idempotent and a new run starts clean (no old samples).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "test_framework.h"

#include "swr_session.h"

/* --- helpers ---------------------------------------------------------------- */

/* A deterministic V-shaped antenna: SWR 1 at f0, rising by |df| / slope. The
 * valley lands on a grid point of a 100 kHz span centred on f0. */
static float antenna_swr(uint32_t freq_hz, uint32_t f0, float slope) {
    float df = (float) ((int64_t) freq_hz - (int64_t) f0);

    if (df < 0) {
        df = -df;
    }

    return 1.0f + df / slope;
}

/* Deterministic per-(point, pass) noise, no rand()/time: same seed -> same run. */
static float fake_noise(uint32_t seed, int point, int pass) {
    uint32_t x = seed * 2654435761u + (uint32_t) point * 40503u + (uint32_t) pass * 97u;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    /* -0.05 .. +0.05, in steps that keep the value off a pure 0.1 grid so the
     * median still sees a real spread. */
    return ((float) (x % 2001) - 1000.0f) / 10000.0f * 0.5f;
}

/* The synthetic antenna reading is clamped to the physical floor (SWR >= 1):
 * the noise can dip below 1 near the valley, and a sub-unity value is not a
 * measurement. */
static float synthetic_swr(uint32_t freq_hz, uint32_t f0, float slope,
                           uint32_t seed, int point, int pass) {
    float v = antenna_swr(freq_hz, f0, slope) + fake_noise(seed, point, pass);

    return (v < 1.0f) ? 1.0f : v;
}

static bool float_eq(float a, float b) {
    return fabsf(a - b) < 1e-6f;
}

/* Intervalo virtual entre paquetes del flow (dwell del barrido, ms): el reloj lo
 * mueve el test, sin sleeps reales. */
#define STEP_MS 35u

/* --- grid ------------------------------------------------------------------- */

void test_swr_grid(void) {
    const uint32_t center = 7100000;    /* 40 m */

    static const struct {
        int32_t span_hz;
        uint32_t step_hz;
    } table[] = {
        {  50000,  1000 },
        { 100000,  2000 },
        { 200000,  4000 },
        { 500000, 10000 },
    };

    for (size_t t = 0; t < sizeof(table) / sizeof(table[0]); t++) {
        swr_grid_t g;

        CHECK(swr_grid_build(center, table[t].span_hz, SWR_SESSION_POINTS, &g) == SWR_GRID_OK);
        CHECK(g.points == 51);
        CHECK(g.step_hz == table[t].step_hz);

        /* both endpoints, inclusive */
        CHECK(g.freq_hz[0] == center - (uint32_t) table[t].span_hz / 2);
        CHECK(g.freq_hz[50] == center + (uint32_t) table[t].span_hz / 2);

        /* exactly the adaptive step, monotone and unique */
        for (int i = 1; i < 51; i++) {
            CHECK(g.freq_hz[i] - g.freq_hz[i - 1] == table[t].step_hz);
        }
    }

    /* Non-divisible span/center: bounded rounding, stable order, no overflow. */
    swr_grid_t a, b;

    CHECK(swr_grid_build(7100000, 50001, 51, &a) == SWR_GRID_OK);
    CHECK(swr_grid_build(7100000, 50001, 51, &b) == SWR_GRID_OK);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);                       /* deterministic */
    CHECK(a.freq_hz[0] == 7100000 - 50001 / 2);
    CHECK(a.freq_hz[50] == a.freq_hz[0] + 50001);
    for (int i = 1; i < 51; i++) {
        CHECK(a.freq_hz[i] > a.freq_hz[i - 1]);                  /* monotone */
    }

    /* Out of the legal TX window -> refused (never swept silently out of band). */
    swr_grid_t g;

    CHECK(swr_grid_build(400000, 200000, 51, &g) == SWR_GRID_RANGE);      /* below 500 kHz */
    CHECK(swr_grid_build(55000000, 500000, 51, &g) == SWR_GRID_RANGE);   /* above 55 MHz */
    CHECK(swr_grid_build(200, 500000, 51, &g) == SWR_GRID_OVERFLOW);     /* underflow */
    CHECK(swr_grid_build(7100000, 0, 51, &g) == SWR_GRID_BAD_PARAMS);
    CHECK(swr_grid_build(7100000, -1000, 51, &g) == SWR_GRID_BAD_PARAMS);
    CHECK(swr_grid_build(7100000, 100000, 1, &g) == SWR_GRID_BAD_PARAMS);
    CHECK(swr_grid_build(7100000, 40, 51, &g) == SWR_GRID_BAD_PARAMS);   /* too narrow for 51 */
}

/* --- validity / aggregation -------------------------------------------------- */

void test_swr_aggregation(void) {
    /* Missing / non-finite / sub-unity readings are invalid; they never become 1.0. */
    CHECK(!swr_sample_valid(NAN));
    CHECK(!swr_sample_valid(INFINITY));
    CHECK(!swr_sample_valid(-INFINITY));
    CHECK(!swr_sample_valid(0.0f));
    CHECK(!swr_sample_valid(0.9f));
    CHECK(!swr_sample_valid(26.0f));
    CHECK(swr_sample_valid(1.0f));
    CHECK(swr_sample_valid(1.2f));
    CHECK(swr_sample_valid(9.9f));

    /* Outlier [1.2, 4.6, 1.2] -> median 1.2 (the mean would be 2.33). */
    swr_point_t p;
    memset(&p, 0, sizeof(p));
    p.samples[0] = (swr_sample_t) { .swr = 1.2f, .valid = true };
    p.samples[1] = (swr_sample_t) { .swr = 4.6f, .valid = true };
    p.samples[2] = (swr_sample_t) { .swr = 1.2f, .valid = true };
    p.n_valid = 3;

    swr_point_stat_t st;
    swr_point_stat(&p, &st);
    CHECK(float_eq(st.median, 1.2f));
    CHECK(st.n_valid == 3);
    CHECK(float_eq(st.mad, 0.0f));    /* |1.2-1.2|, |4.6-1.2|, |1.2-1.2| -> median 0.0 */

    /* Quantization: repeated 0.1 values are reported honestly (MAD 0, not a fake precision). */
    swr_point_t q;
    memset(&q, 0, sizeof(q));
    q.samples[0] = (swr_sample_t) { .swr = 1.5f, .valid = true };
    q.samples[1] = (swr_sample_t) { .swr = 1.5f, .valid = true };
    q.samples[2] = (swr_sample_t) { .swr = 1.6f, .valid = true };
    q.n_valid = 3;

    swr_point_stat(&q, &st);
    CHECK(float_eq(st.median, 1.5f));
    CHECK(float_eq(st.mad, 0.0f));

    /* Invalid samples are excluded from the median, not coerced. */
    swr_point_t m;
    memset(&m, 0, sizeof(m));
    m.samples[0] = (swr_sample_t) { .swr = 1.3f, .valid = true };
    m.samples[1] = (swr_sample_t) { .swr = NAN, .valid = false };
    m.samples[2] = (swr_sample_t) { .swr = 0.0f, .valid = false };
    m.n_valid = 1;
    m.n_invalid = 2;

    swr_point_stat(&m, &st);
    CHECK(float_eq(st.median, 1.3f));
    CHECK(st.n_valid == 1);
    CHECK(isnan(st.mad));            /* a single sample has no spread */

    /* No valid sample at all: absent, NOT 1.0. */
    swr_point_t e;
    memset(&e, 0, sizeof(e));
    e.samples[0] = (swr_sample_t) { .swr = NAN, .valid = false };
    e.n_invalid = 1;

    swr_point_stat(&e, &st);
    CHECK(isnan(st.median));
    CHECK(st.n_valid == 0);

    /* Saturation is flagged, not silently trusted. */
    swr_point_t s;
    memset(&s, 0, sizeof(s));
    s.samples[0] = (swr_sample_t) { .swr = 9.9f, .valid = true };
    s.saturated = true;
    s.n_valid = 1;

    swr_point_stat(&s, &st);
    CHECK(st.saturated);
}

/* --- one pass, the full sweep ----------------------------------------------- */

void test_swr_session_quick(void) {
    swr_grid_t g;

    CHECK(swr_grid_build(7100000, 100000, SWR_SESSION_POINTS, &g) == SWR_GRID_OK);

    swr_limits_t lim;
    swr_limits_default(&lim);
    lim.max_passes = SWR_SESSION_MAX_PASSES;

    swr_session_t s;
    uint32_t      f = 0;

    CHECK(swr_session_begin(&s, &g, SWR_SESSION_PASSES_QUICK, &lim, 0, &f) == SWR_ACT_SET_FREQ);
    CHECK(s.state == SWR_SESSION_SCANNING);
    CHECK(f == g.freq_hz[0]);

    uint32_t t = 0;

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        swr_action_t act = swr_session_packet(&s, 1.5f, t, &f);

        t += STEP_MS;

        if (i < SWR_SESSION_POINTS - 1) {
            CHECK(act == SWR_ACT_SET_FREQ);
            CHECK(f == g.freq_hz[i + 1]);            /* exact same grid, in order */
        } else {
            CHECK(act == SWR_ACT_COMPLETE);          /* the sweep ends by itself */
        }
    }

    CHECK(s.state == SWR_SESSION_COMPLETED);
    CHECK(s.passes_completed == 1);
    CHECK(swr_session_valid_points(&s) == SWR_SESSION_POINTS);

    /* A packet after completion does nothing (the TX is already off). */
    CHECK(swr_session_packet(&s, 1.5f, t, &f) == SWR_ACT_NONE);
    CHECK(s.packets == SWR_SESSION_POINTS);
}

/* --- multipass: same frequencies, per-index aggregation ---------------------- */

void test_swr_session_multipass(void) {
    const uint32_t f0 = 7100000;

    swr_grid_t g;

    CHECK(swr_grid_build(f0, 100000, SWR_SESSION_POINTS, &g) == SWR_GRID_OK);

    /* f0 must be a grid point (index 25 for 100 kHz / 51 points). */
    CHECK(g.freq_hz[25] == f0);

    swr_limits_t lim;
    swr_limits_default(&lim);
    lim.max_passes = SWR_SESSION_MAX_PASSES;

    swr_session_t s;
    uint32_t      f = 0;

    CHECK(swr_session_begin(&s, &g, SWR_SESSION_PASSES_STABLE, &lim, 0, &f) == SWR_ACT_SET_FREQ);

    uint32_t t = 0;

    for (int pass = 0; pass < SWR_SESSION_PASSES_STABLE; pass++) {
        for (int i = 0; i < SWR_SESSION_POINTS; i++) {
            float swr = synthetic_swr(g.freq_hz[i], f0, 20000.0f, 1234u, i, pass);
            swr_action_t act = swr_session_packet(&s, swr, t, &f);

            t += STEP_MS;

            if (i < SWR_SESSION_POINTS - 1) {
                CHECK(act == SWR_ACT_SET_FREQ);
                CHECK(f == g.freq_hz[i + 1]);
            } else if (pass < SWR_SESSION_PASSES_STABLE - 1) {
                CHECK(act == SWR_ACT_SET_FREQ);
                CHECK(f == g.freq_hz[0]);            /* next pass restarts at the low edge */
                CHECK(s.pass == pass + 1);
            } else {
                CHECK(act == SWR_ACT_COMPLETE);
            }
        }
    }

    CHECK(s.state == SWR_SESSION_COMPLETED);
    CHECK(s.passes_completed == 3);

    /* Every index holds up to 3 samples of ITS OWN frequency: the stored freq is
     * the grid freq and every sample belongs to that index (no mixing). */
    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        CHECK(s.points[i].freq_hz == g.freq_hz[i]);
        CHECK(s.points[i].n_valid == 3);
    }

    /* The measured minimum is the valley, within one grid cell (the noise near
     * the floor can tie a neighbour at SWR 1.0). */
    int   mp;
    float mswr;
    float mmad;

    CHECK(swr_session_minimum(&s, &mp, &mswr, &mmad));
    CHECK(mp >= 24 && mp <= 26);                 /* f0 is grid index 25 */
    CHECK(mswr >= 1.0f && mswr < 1.2f);          /* the valley is ~1.0 */
    CHECK(mmad >= 0.0f);                          /* a spread is reported, not invented */

    /* A false low spike must not steal the minimum: with an isolated very low
     * reading at a far point, the MEDIAN of that point stays high, so the valley
     * (a consistent low across passes) still wins. */
    swr_point_t spike;
    memset(&spike, 0, sizeof(spike));
    spike.samples[0] = (swr_sample_t) { .swr = 1.05f, .valid = true };
    spike.samples[1] = (swr_sample_t) { .swr = 4.5f, .valid = true };
    spike.samples[2] = (swr_sample_t) { .swr = 4.5f, .valid = true };
    spike.n_valid = 3;

    swr_point_stat_t st;
    swr_point_stat(&spike, &st);
    CHECK(float_eq(st.median, 4.5f));            /* not the 1.05 spike */
}

/* --- alternating direction: storage index and frequency stay tied ------------ */

void test_swr_session_alternate(void) {
    swr_grid_t g;

    CHECK(swr_grid_build(7100000, 50000, SWR_SESSION_POINTS, &g) == SWR_GRID_OK);

    swr_limits_t lim;
    swr_limits_default(&lim);
    lim.max_passes          = SWR_SESSION_MAX_PASSES;
    lim.alternate_direction = true;

    swr_session_t s;
    uint32_t      f = 0;

    CHECK(swr_session_begin(&s, &g, 2, &lim, 0, &f) == SWR_ACT_SET_FREQ);
    CHECK(f == g.freq_hz[0]);                   /* pass 0: low -> high */
    CHECK(s.point == 0);

    uint32_t t = 0;

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        /* A value that encodes its own grid index, so a shifted sample is visible. */
        float swr = 1.0f + 0.1f * (float) i;

        CHECK(f == g.freq_hz[i]);

        swr_action_t act = swr_session_packet(&s, swr, t, &f);
        t += STEP_MS;

        if (i < SWR_SESSION_POINTS - 1) {
            CHECK(act == SWR_ACT_SET_FREQ);
        } else {
            /* pass 1 starts at the HIGH edge (descending). */
            CHECK(act == SWR_ACT_SET_FREQ);
            CHECK(f == g.freq_hz[SWR_SESSION_POINTS - 1]);
            CHECK(s.point == SWR_SESSION_POINTS - 1);
            CHECK(s.point_seq == 0);
        }
    }

    for (int k = 0; k < SWR_SESSION_POINTS; k++) {
        int idx = SWR_SESSION_POINTS - 1 - k;

        /* The sweep cursor IS the grid index: descending just walks it backwards. */
        CHECK(s.point == idx);
        CHECK(f == g.freq_hz[idx]);

        float swr = 2.0f + 0.1f * (float) idx;

        swr_action_t act = swr_session_packet(&s, swr, t, &f);
        t += STEP_MS;

        if (k == SWR_SESSION_POINTS - 1) {
            CHECK(act == SWR_ACT_COMPLETE);
        }
    }

    /* Each index still owns its frequency AND its two samples: the descending
     * pass did not shift the values by one index (the #125 bug this pins). */
    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        CHECK(s.points[i].freq_hz == g.freq_hz[i]);
        CHECK(s.points[i].n_valid == 2);
        CHECK(s.points[i].samples[0].valid);
        CHECK(s.points[i].samples[1].valid);
        CHECK(float_eq(s.points[i].samples[0].swr, 1.0f + 0.1f * (float) i));
        CHECK(float_eq(s.points[i].samples[1].swr, 2.0f + 0.1f * (float) i));
    }

    CHECK(s.point_seq == SWR_SESSION_POINTS);
    CHECK(s.state == SWR_SESSION_COMPLETED);
}

/* --- settle window: stale packets are discarded ------------------------------ */

void test_swr_session_settle(void) {
    swr_grid_t g;

    CHECK(swr_grid_build(7100000, 100000, SWR_SESSION_POINTS, &g) == SWR_GRID_OK);

    swr_limits_t lim;
    swr_limits_default(&lim);
    lim.max_passes = 1;
    lim.settle_ms  = 100;

    swr_session_t s;
    uint32_t      f = 0;

    CHECK(swr_session_begin(&s, &g, 1, &lim, 1000, &f) == SWR_ACT_SET_FREQ);

    /* Packets younger than 100 ms after the frequency was commanded may still
     * carry the previous frequency: they are discarded and do NOT advance. */
    CHECK(swr_session_packet(&s, 9.9f, 1020, &f) == SWR_ACT_NONE);
    CHECK(swr_session_packet(&s, 9.9f, 1099, &f) == SWR_ACT_NONE);
    CHECK(s.discarded == 2);
    CHECK(s.points[0].n_valid == 0);            /* the stale value was NOT stored */

    /* The first packet at the window edge is the real sample of point 0. */
    CHECK(swr_session_packet(&s, 1.3f, 1100, &f) == SWR_ACT_SET_FREQ);
    CHECK(f == g.freq_hz[1]);
    CHECK(s.points[0].n_valid == 1);
    CHECK(float_eq(s.points[0].samples[0].swr, 1.3f));

    /* The window is measured from the last set_freq; swr_session_tuned moves that
     * reference to the instant the caller actually commanded the VFO. */
    swr_session_tuned(&s, 2000);
    CHECK(swr_session_packet(&s, 9.9f, 2050, &f) == SWR_ACT_NONE);
    CHECK(swr_session_packet(&s, 1.4f, 2100, &f) == SWR_ACT_SET_FREQ);
    CHECK(s.points[1].n_valid == 1);
    CHECK(float_eq(s.points[1].samples[0].swr, 1.4f));

    /* Without a settle window the first packet is attributed (the #113 behavior,
     * now explicit): no discard. */
    lim.settle_ms = 0;
    CHECK(swr_session_begin(&s, &g, 1, &lim, 5000, &f) == SWR_ACT_SET_FREQ);
    CHECK(swr_session_packet(&s, 9.9f, 5001, &f) == SWR_ACT_SET_FREQ);
    CHECK(s.discarded == 0);
    CHECK(s.points[0].n_valid == 1);
    CHECK(float_eq(s.points[0].samples[0].swr, 9.9f));
}

/* --- stop / abort / timeout -------------------------------------------------- */

void test_swr_session_lifecycle(void) {
    swr_grid_t g;

    CHECK(swr_grid_build(7100000, 100000, SWR_SESSION_POINTS, &g) == SWR_GRID_OK);

    swr_limits_t lim;
    swr_limits_default(&lim);
    lim.max_passes         = SWR_SESSION_MAX_PASSES;
    lim.pass_timeout_ms    = 2000;
    lim.session_timeout_ms = 4000;

    swr_session_t s;
    uint32_t      f = 0;

    /* 3 passes over a radio that only allows 1 -> gated, no sweep. */
    swr_limits_t one;
    swr_limits_default(&one);
    one.max_passes = 1;

    swr_session_t gated;

    CHECK(swr_session_begin(&gated, &g, 3, &one, 0, &f) == SWR_ACT_ABORT);
    CHECK(gated.state == SWR_SESSION_ERROR);
    CHECK(gated.abort_reason == SWR_ABORT_MULTIPASS_GATED);

    /* Stop mid-pass keeps the partial data and is idempotent. */
    CHECK(swr_session_begin(&s, &g, 3, &lim, 0, &f) == SWR_ACT_SET_FREQ);

    for (int i = 0; i < 10; i++) {
        swr_session_packet(&s, 1.4f, (uint32_t) (i * 35), &f);
    }

    CHECK(swr_session_valid_points(&s) == 10);
    CHECK(swr_session_stop(&s, 400) == SWR_ACT_ABORT);
    CHECK(s.state == SWR_SESSION_STOPPED);
    CHECK(s.abort_reason == SWR_ABORT_USER);
    CHECK(swr_session_stop(&s, 500) == SWR_ACT_NONE);       /* idempotent */
    CHECK(swr_session_packet(&s, 1.4f, 600, &f) == SWR_ACT_NONE);
    CHECK(swr_session_valid_points(&s) == 10);              /* partial kept */

    /* A new run starts clean: no samples from the previous session. */
    CHECK(swr_session_begin(&s, &g, 1, &lim, 1000, &f) == SWR_ACT_SET_FREQ);
    CHECK(swr_session_valid_points(&s) == 0);
    CHECK(s.passes_completed == 0);
    CHECK(s.packets == 0);
    CHECK(s.state == SWR_SESSION_SCANNING);

    /* Timeout with no packets: the poll watchdog aborts. */
    CHECK(swr_session_poll(&s, 1000 + 1999) == SWR_ACT_NONE);
    CHECK(swr_session_poll(&s, 1000 + 2001) == SWR_ACT_ABORT);
    CHECK(s.state == SWR_SESSION_ERROR);
    CHECK(s.abort_reason == SWR_ABORT_TIMEOUT);
    CHECK(swr_session_poll(&s, 99999) == SWR_ACT_NONE);     /* terminal: no-op */

    /* Abort while scanning -> Error with the reason. */
    CHECK(swr_session_begin(&s, &g, 1, &lim, 0, &f) == SWR_ACT_SET_FREQ);
    swr_session_abort(&s, SWR_ABORT_STATE_CHANGED, 10);
    CHECK(s.state == SWR_SESSION_ERROR);
    CHECK(s.abort_reason == SWR_ABORT_STATE_CHANGED);
}

/* --- two valleys / flat / no valley ------------------------------------------ */

void test_swr_session_shapes(void) {
    swr_grid_t g;

    CHECK(swr_grid_build(7100000, 100000, SWR_SESSION_POINTS, &g) == SWR_GRID_OK);

    swr_limits_t lim;
    swr_limits_default(&lim);
    lim.max_passes = 1;

    swr_session_t s;
    uint32_t      f = 0;

    /* Two valleys of different depth: the minimum is the deeper one, read from the
     * measured data (never a single global fit). */
    CHECK(swr_session_begin(&s, &g, 1, &lim, 0, &f) == SWR_ACT_SET_FREQ);

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        float swr = 2.0f;

        if (i == 10) {
            swr = 1.4f;         /* valley A */
        } else if (i == 40) {
            swr = 1.1f;         /* valley B: deeper */
        }

        swr_session_packet(&s, swr, (uint32_t) (i * STEP_MS), &f);
    }

    int   mp = -1;
    float mswr = 0.0f;
    float mmad = 0.0f;

    CHECK(swr_session_minimum(&s, &mp, &mswr, &mmad));
    CHECK(mp == 40);
    CHECK(float_eq(mswr, 1.1f));

    /* Flat curve: the minimum is the (single) level, no invented valley. */
    CHECK(swr_session_begin(&s, &g, 1, &lim, 0, &f) == SWR_ACT_SET_FREQ);

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        swr_session_packet(&s, 2.0f, (uint32_t) (i * STEP_MS), &f);
    }

    CHECK(swr_session_minimum(&s, &mp, &mswr, &mmad));
    CHECK(float_eq(mswr, 2.0f));
    CHECK(mp == 0);                 /* first tie wins, deterministically */

    /* No valid data at all: no minimum, and the readout would say "sin datos". */
    CHECK(swr_session_begin(&s, &g, 1, &lim, 0, &f) == SWR_ACT_SET_FREQ);

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        swr_session_packet(&s, 0.0f, (uint32_t) (i * STEP_MS), &f);   /* missing */
    }

    CHECK(!swr_session_minimum(&s, &mp, &mswr, &mmad));
    CHECK(swr_session_valid_points(&s) == 0);

    /* A minimum at the edge is respected (no forced interior valley). */
    CHECK(swr_session_begin(&s, &g, 1, &lim, 0, &f) == SWR_ACT_SET_FREQ);

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        swr_session_packet(&s, (i == 0) ? 1.05f : 2.0f, (uint32_t) (i * STEP_MS), &f);
    }

    CHECK(swr_session_minimum(&s, &mp, &mswr, &mmad));
    CHECK(mp == 0);
    CHECK(float_eq(mswr, 1.05f));
}

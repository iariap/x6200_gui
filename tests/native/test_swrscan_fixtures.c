/*
 * Fixture-driven tests for the SWR scan session engine (src/swr_session.c,
 * issue #125 section 7).
 *
 * The owner delivered ready-to-use, deterministic datasets under
 * tests/fixtures/swrscan/ (a samples table, the exact per-point expected
 * aggregates, a scenario/ground-truth JSON and a command/packet flow with a
 * stale reading after every frequency change). These tests replay them through
 * the real engine and compare against the delivered tables, instead of
 * inventing curves that would let the code pass. The fixture README is the
 * contract; nothing here regenerates a fixture.
 *
 * What is pinned:
 *   - every scenario's grid is exactly the fixture's 51 frequencies (built from
 *     center/span), and the engine sweeps them in the fixture's acquisition
 *     order (odd passes backward);
 *   - the per-point median and MAD match swr_expected_aggregates.csv for ALL 408
 *     points (the median rejects the isolated outliers, the MAD reports the real
 *     spread of the moving/flat cases honestly);
 *   - the measured minimum is the fixture's ground-truth minimum (never a
 *     parabola vertex), including the cases where it must NOT claim a single
 *     sharp resonance (flat / wide zoom) and the two-valley case;
 *   - replaying swr_mock_delayed_flow.csv through the engine with a settle window
 *     discards every stale packet and never advances on one (the sample of each
 *     (pass, grid point) is the VALID packet's value, not the stale one that
 *     arrived right after the frequency change).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_framework.h"

#include "swr_session.h"
#include "swrscan_fixtures.h"

static swr_fx_sample_t    samples[SWR_FX_MAX_SAMPLES];
static swr_fx_aggregate_t aggregates[SWR_FX_MAX_AGGREGATES];
static swr_fx_flow_event_t flow[SWR_FX_MAX_FLOW];

static int n_samples;
static int n_aggregates;
static int n_flow;

/* The scenarios of swr_scenarios.json, in fixture order. The passes count is
 * derived from the samples (see scenario_passes), not hardcoded here. */
static const char *const scenario_names[] = {
    "single_valley_3pass",
    "outliers_5pass",
    "moving_5pass",
    "double_valley_3pass",
    "flat_3pass",
    "narrow_zoom_3pass",
    "medium_zoom_3pass",
    "wide_zoom_3pass",
};

#define N_SCENARIOS ((int) (sizeof(scenario_names) / sizeof(scenario_names[0])))

static int scenario_passes(const char *name) {
    int max_pass = -1;

    for (int i = 0; i < n_samples; i++) {
        if (strcmp(samples[i].scenario, name) == 0 && samples[i].pass_index > max_pass) {
            max_pass = samples[i].pass_index;
        }
    }

    return max_pass + 1;
}

/* Rebuilds the grid from the fixture: the samples of a point must all agree on
 * their frequency, and the 51 frequencies must be exactly what
 * swr_grid_build(center, span, 51) produces. */
static bool scenario_grid(const char *name, swr_grid_t *g) {
    uint32_t freq[SWR_SESSION_POINTS];
    bool     seen[SWR_SESSION_POINTS];

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        freq[i] = 0;
        seen[i] = false;
    }

    for (int i = 0; i < n_samples; i++) {
        if (strcmp(samples[i].scenario, name) != 0) {
            continue;
        }

        int p = samples[i].point_index;

        if (p < 0 || p >= SWR_SESSION_POINTS) {
            return false;
        }

        if (seen[p] && freq[p] != samples[i].freq_hz) {
            return false;   /* a point with two different frequencies: broken fixture */
        }

        freq[p] = samples[i].freq_hz;
        seen[p] = true;
    }

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        if (!seen[i]) {
            return false;
        }
    }

    uint32_t span   = freq[SWR_SESSION_POINTS - 1] - freq[0];
    uint32_t center = freq[0] + span / 2;

    if (swr_grid_build(center, (int32_t) span, SWR_SESSION_POINTS, g) != SWR_GRID_OK) {
        return false;
    }

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        if (g->freq_hz[i] != freq[i]) {
            return false;
        }
    }

    return true;
}

static bool same_tenths(float v, int tenths) {
    return !isnan(v) && lroundf(v * 10.0f) == tenths;
}

/* Feeds every sample of `name` to a fresh session, in the fixture's acquisition
 * order (pass by pass, order_in_pass within the pass). Returns the completed
 * session and asserts the engine requested exactly the fixture's frequency at
 * every step. */
static void replay_samples(const char *name, swr_session_t *s, swr_grid_t *g) {
    CHECK(scenario_grid(name, g));

    int passes = scenario_passes(name);

    CHECK(passes == 3 || passes == 5);

    swr_limits_t lim;

    swr_limits_default(&lim);
    lim.max_passes          = SWR_SESSION_MAX_PASSES;
    lim.alternate_direction = true;     /* odd passes run backward in the fixtures */

    uint32_t    f   = 0;
    swr_action_t act = swr_session_begin(s, g, passes, &lim, 0, &f);

    CHECK(act == SWR_ACT_SET_FREQ);

    for (int pass = 0; pass < passes; pass++) {
        for (int order = 0; order < SWR_SESSION_POINTS; order++) {
            const swr_fx_sample_t *sample = NULL;

            for (int i = 0; i < n_samples; i++) {
                if (strcmp(samples[i].scenario, name) == 0 && samples[i].pass_index == pass
                    && samples[i].order_in_pass == order) {
                    sample = &samples[i];
                    break;
                }
            }

            CHECK(sample != NULL);

            if (!sample) {
                return;
            }

            /* The frequency the engine asked for is the fixture's frequency for
             * this acquisition step: no grid drift, no index/frequency mix-up on
             * the descending passes. */
            CHECK(f == sample->freq_hz);
            CHECK(s->points[sample->point_index].freq_hz == sample->freq_hz);

            act = swr_session_packet(s, (float) sample->raw_tenths * 0.1f,
                                     sample->timestamp_ms, &f);

            if (pass == passes - 1 && order == SWR_SESSION_POINTS - 1) {
                CHECK(act == SWR_ACT_COMPLETE);
            } else {
                CHECK(act == SWR_ACT_SET_FREQ);
            }
        }
    }

    CHECK(s->state == SWR_SESSION_COMPLETED);
    CHECK(s->passes_completed == passes);
    CHECK(swr_session_valid_points(s) == SWR_SESSION_POINTS);
}

/* --- the per-point aggregate oracle ----------------------------------------- */

void test_swr_fixture_aggregates(void) {
    n_samples    = swr_fx_load_samples(samples, SWR_FX_MAX_SAMPLES);
    n_aggregates = swr_fx_load_aggregates(aggregates, SWR_FX_MAX_AGGREGATES);

    CHECK(n_samples == 1428);
    CHECK(n_aggregates == 408);

    if (n_samples < 0 || n_aggregates < 0) {
        return;
    }

    for (int sc = 0; sc < N_SCENARIOS; sc++) {
        const char  *name = scenario_names[sc];
        swr_session_t s;
        swr_grid_t    g;

        replay_samples(name, &s, &g);

        for (int i = 0; i < n_aggregates; i++) {
            if (strcmp(aggregates[i].scenario, name) != 0) {
                continue;
            }

            int p = aggregates[i].point_index;

            swr_point_stat_t st;

            CHECK(swr_session_point_stat(&s, p, &st));
            CHECK(s.points[p].freq_hz == aggregates[i].freq_hz);
            CHECK(st.n_valid == aggregates[i].valid_samples);
            CHECK(same_tenths(st.median, aggregates[i].median_tenths));
            CHECK(same_tenths(st.mad, aggregates[i].mad_tenths));
        }
    }

    printf("fixtures: %d escenarios, %d puntos agregados comparados contra el oraculo\n",
           N_SCENARIOS, n_aggregates);
}

/* --- the measured minimum (never a parabola vertex) ------------------------- */

void test_swr_fixture_minimum(void) {
    struct {
        const char *name;
        int         point;
        int         swr_tenths;
        int         min_mad_tenths;   /* -1: no lower bound */
        int         distinct_medians; /* 1 means "no unique resonance" */
    } table[] = {
        { "single_valley_3pass", 25, 12, -1, -1 },   /* dip at 7.100 MHz */
        { "outliers_5pass",     25, 12, -1, -1 },   /* the outliers do not move it */
        { "double_valley_3pass", 15, 13, -1, -1 },  /* the deeper of two valleys */
        { "narrow_zoom_3pass",  28, 12, -1, -1 },   /* 7.103 MHz resolved at 1 kHz */
        { "medium_zoom_3pass",  25, 12, -1, -1 },   /* 4 kHz spacing */
        { "flat_3pass",          0, 26, -1,  1 },   /* no unique resonance */
        { "wide_zoom_3pass",     0, 35, -1,  1 },   /* the 7.103 dip is missed */
    };

    for (size_t t = 0; t < sizeof(table) / sizeof(table[0]); t++) {
        swr_session_t s;
        swr_grid_t    g;

        replay_samples(table[t].name, &s, &g);

        int   point = -1;
        float swr   = 0.0f;
        float mad   = 0.0f;

        CHECK(swr_session_minimum(&s, &point, &swr, &mad));
        CHECK(point == table[t].point);
        CHECK(same_tenths(swr, table[t].swr_tenths));

        if (table[t].distinct_medians == 1) {
            /* Every point has the same median: the data has no single credible
             * resonance and the engine must not invent one. */
            int distinct = 0;

            for (int i = 0; i < SWR_SESSION_POINTS; i++) {
                swr_point_stat_t st;

                swr_session_point_stat(&s, i, &st);

                bool seen = false;

                for (int j = 0; j < i; j++) {
                    swr_point_stat_t prev;

                    swr_session_point_stat(&s, j, &prev);

                    if (same_tenths(st.median, lroundf(prev.median * 10.0f))) {
                        seen = true;
                        break;
                    }
                }

                if (!seen) {
                    distinct++;
                }
            }

            CHECK(distinct == 1);
        }
    }

    /* Drift (the valley moves between passes): the aggregate minimum is HIGHER
     * than any single pass's minimum and its MAD is not zero — the honest report
     * is instability, not a sharp resonance at one pass's frequency. */
    {
        swr_session_t s;
        swr_grid_t    g;

        replay_samples("moving_5pass", &s, &g);

        int   point = -1;
        float swr   = 0.0f;
        float mad   = 0.0f;

        CHECK(swr_session_minimum(&s, &point, &swr, &mad));
        CHECK(same_tenths(swr, 18));           /* 1.8, not the 1.3 of any single pass */
        CHECK(point >= 23 && point <= 27);     /* the moving valley's middle points */
        CHECK(mad > 0.0f);                     /* the spread is reported */
    }

    /* Two valleys: the minimum is the deeper one, and the second valley is a
     * separate local minimum — the engine does not force a single global fit. */
    {
        swr_session_t s;
        swr_grid_t    g;

        replay_samples("double_valley_3pass", &s, &g);

        int   point = -1;
        float swr   = 0.0f;
        float mad   = 0.0f;

        CHECK(swr_session_minimum(&s, &point, &swr, &mad));
        CHECK(point == 15);
        CHECK(same_tenths(swr, 13));

        swr_point_stat_t second;

        CHECK(swr_session_point_stat(&s, 35, &second));
        CHECK(same_tenths(second.median, 19));      /* the second valley is real too */
        CHECK(second.median > swr);                 /* but the first one is deeper */
    }
}

/* --- the delayed flow: stale packets are discarded, never misattributed ------ */

void test_swr_fixture_delayed_flow(void) {
    n_flow = swr_fx_load_flow(flow, SWR_FX_MAX_FLOW);

    CHECK(n_flow == 459);

    if (n_flow < 0) {
        return;
    }

    /* The flow is 3 passes of 51 points, each with a set_frequency, a stale
     * packet (+20 ms) and a valid one (+130 ms). */
    swr_grid_t g;

    CHECK(swr_grid_build(7100000, 100000, SWR_SESSION_POINTS, &g) == SWR_GRID_OK);

    swr_limits_t lim;

    swr_limits_default(&lim);
    lim.max_passes          = SWR_SESSION_MAX_PASSES;
    lim.alternate_direction = true;
    lim.settle_ms           = 100;   /* test policy from the fixture README, not a radio value */

    swr_session_t s;
    uint32_t      f   = 0;
    swr_action_t  act = swr_session_begin(&s, &g, 3, &lim, flow[0].ts_ms, &f);

    CHECK(act == SWR_ACT_SET_FREQ);

    int stale_packets = 0;

    for (int i = 0; i < n_flow; i++) {
        const swr_fx_flow_event_t *e = &flow[i];

        if (e->is_set_freq) {
            /* The engine asked for exactly this frequency and the flow commands
             * it now: tell the engine when, so the settle window is measured
             * from the real command. */
            CHECK(f == e->target_freq_hz);
            swr_session_tuned(&s, e->ts_ms);
            continue;
        }

        act = swr_session_packet(&s, (float) e->swr_tenths * 0.1f, e->ts_ms, &f);

        if (e->stale) {
            /* A stale packet must NOT be stored and must NOT advance the sweep. */
            CHECK(act == SWR_ACT_NONE);
            CHECK(s.point == e->point_index || s.point == e->point_index + 1);
            stale_packets++;
        } else {
            /* The valid packet is stored at the point the flow says, with the
             * value that really belongs to the requested frequency. */
            CHECK(s.points[e->point_index].samples[e->pass_index].valid);
            CHECK(same_tenths(s.points[e->point_index].samples[e->pass_index].swr, e->swr_tenths));
        }
    }

    CHECK(stale_packets == 153);           /* one per point per pass */
    CHECK(s.discarded == 153);
    CHECK(s.state == SWR_SESSION_COMPLETED);
    CHECK(s.passes_completed == 3);

    /* Every grid point holds exactly 3 valid samples, all of ITS OWN frequency:
     * the stale packets never landed on the next point. */
    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        CHECK(s.points[i].freq_hz == g.freq_hz[i]);
        CHECK(s.points[i].n_valid == 3);
        CHECK(s.points[i].n_invalid == 0);
    }

    printf("flow de fixture: 459 eventos, %d paquetes stale descartados, 0 atribuidos a la "
           "frecuencia nueva, 51x3 muestras validas\n", stale_packets);
}

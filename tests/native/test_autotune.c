/*
 * Unit tests: the fine-tuning assistant core (issue #100) - scoring, the
 * zigzag probe plan and the deterministic restore. No LVGL, no radio: the
 * whole algorithm runs against synthetic fixtures.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>

#include "autotune.h"
#include "test_framework.h"

/* --- scoring ------------------------------------------------------------- */

static void test_score_median_odd(void) {
    const int16_t a[] = { -81, -73, -95 };
    const int16_t b[] = { 5, 1, 100 };

    /* The median, not the mean: a single spike does not move it. */
    CHECK(autotune_score(a, 3) == -81);
    CHECK(autotune_score(b, 3) == 5);
}

static void test_score_median_even(void) {
    const int16_t a[] = { -80, -70, -90, -60 };   /* sorted -90 -80 -70 -60 */

    CHECK(autotune_score(a, 4) == (-80 + -70) / 2);
}

static void test_score_unsorted_and_empty(void) {
    const int16_t a[] = { -95, -81, -73 };

    CHECK(autotune_score(a, 3) == -81);
    CHECK(autotune_score(a, 0) == INT16_MIN);
    CHECK(autotune_score(NULL, 0) == INT16_MIN);
}

/* --- probe plan ---------------------------------------------------------- */

static void test_plan_zigzag(void) {
    autotune_t at;
    const int32_t start = 14074000;

    CHECK(autotune_plan(&at, start, 500, 5000) == 21);
    CHECK(at.count == 21);
    CHECK(at.probes[0] == start);          /* the start is measured too */
    CHECK(at.probes[1] == start + 500);
    CHECK(at.probes[2] == start - 500);
    CHECK(at.probes[3] == start + 1000);
    CHECK(at.probes[4] == start - 1000);
    CHECK(at.probes[19] == start + 5000);
    CHECK(at.probes[20] == start - 5000);
    CHECK(at.start_freq == start);
}

static void test_plan_window_not_multiple_of_step(void) {
    autotune_t at;

    /* 1200 Hz window with a 500 Hz step stops before 1500 (which is outside). */
    CHECK(autotune_plan(&at, 7000000, 500, 1200) == 5);
    CHECK(at.probes[4] == 7000000 - 1000);
}

static void test_plan_defaults_and_clamp(void) {
    autotune_t at;

    /* step <= 0 falls back to the default; a window of 0 plans only the start. */
    CHECK(autotune_plan(&at, 10000000, 0, 0) == 1);
    CHECK(at.probes[0] == 10000000);

    /* A huge window never overflows the fixed probe buffer. */
    CHECK(autotune_plan(&at, 10000000, 100, 100000000) == AUTOTUNE_MAX_PROBES);
}

/* --- measurement state machine ------------------------------------------- */

static void test_feed_completes_after_k_samples(void) {
    autotune_t at;

    autotune_plan(&at, 14074000, 500, 1000);
    CHECK(autotune_running(&at));
    CHECK(autotune_probe_freq(&at) == 14074000);

    for (int i = 0; i < AUTOTUNE_SAMPLES - 1; i++) {
        CHECK(!autotune_feed(&at, -80));
    }
    CHECK(autotune_feed(&at, -80));       /* the k-th sample completes it */
    CHECK(at.scores[0] == -80);
    CHECK(at.completed == 1);
}

static void test_next_walks_the_plan(void) {
    autotune_t at;

    autotune_plan(&at, 14074000, 500, 1000);   /* start, +500, -500, +1000, -1000 */
    CHECK(at.count == 5);

    for (int i = 0; i < AUTOTUNE_SAMPLES; i++) {
        autotune_feed(&at, -80);
    }
    CHECK(autotune_next(&at));
    CHECK(autotune_probe_freq(&at) == 14074500);

    /* Walk to the last probe; the next call runs off the end. */
    for (int p = 1; p < at.count; p++) {
        for (int i = 0; i < AUTOTUNE_SAMPLES; i++) {
            autotune_feed(&at, -80);
        }
        if (p + 1 < at.count) {
            CHECK(autotune_next(&at));
        }
    }
    CHECK(!autotune_next(&at));
    CHECK(!autotune_running(&at));
}

/* --- decision / restore -------------------------------------------------- */

/* Feed every probe the given scores and return the decided frequency. */
static int32_t run(const int32_t start, int32_t step, int32_t window,
                   const int16_t *scores, int16_t margin) {
    autotune_t at;

    autotune_plan(&at, start, step, window);
    for (int p = 0; p < at.count; p++) {
        for (int i = 0; i < AUTOTUNE_SAMPLES; i++) {
            autotune_feed(&at, scores[p]);
        }
        autotune_next(&at);
    }
    return autotune_result(&at, margin);
}

static void test_result_picks_the_best_side(void) {
    /* 5 probes: start, +500, -500, +1000, -1000. The strongest is -1000. */
    const int16_t scores[] = { -85, -80, -82, -78, -70 };

    CHECK(run(14074000, 500, 1000, scores, 1) == 14074000 - 1000);
}

static void test_result_restores_when_start_wins(void) {
    const int16_t scores[] = { -70, -80, -82, -78, -75 };

    CHECK(run(14074000, 500, 1000, scores, 1) == 14074000);
}

static void test_result_restores_when_improvement_is_below_margin(void) {
    /* The best side (-79) beats the start (-80) by 1 dB: below a 2 dB margin
     * it does not move. */
    const int16_t scores[] = { -80, -82, -83, -81, -79 };

    CHECK(run(14074000, 500, 1000, scores, 2) == 14074000);
    CHECK(run(14074000, 500, 1000, scores, 1) == 14074000);   /* 1 is not > 1 */
    /* With no margin the same data moves to the best probe. */
    CHECK(run(14074000, 500, 1000, scores, 0) == 14074000 - 1000);
}

static void test_result_restores_without_measurements(void) {
    autotune_t at;

    autotune_plan(&at, 14074000, 500, 1000);
    CHECK(autotune_result(&at, 1) == 14074000);   /* nothing measured: restore */
}

static void test_result_is_deterministic_on_a_tie(void) {
    /* Two probes tie for the best (both -70, above the start -80): the first
     * one in the plan order wins, so the outcome does not depend on the
     * iteration order. The plan is start, +500, -500, +1000, -1000, so the
     * +1000 probe (index 3) is the earliest of the two. */
    const int16_t scores[] = { -80, -75, -72, -70, -70 };

    CHECK(run(14074000, 500, 1000, scores, 1) == 14074000 + 1000);
}

/* --- the whole scan, as the UI drives it --------------------------------- */

static void test_scan_end_to_end(void) {
    autotune_t at;
    const int32_t start = 14074000;
    /* The signal peaks 1500 Hz below the dial (the classic "off by a step"). */
    const int32_t peak = start - 1500;
    int32_t probes_seen = 0;
    int32_t best = start;

    autotune_plan(&at, start, 500, 5000);
    while (autotune_running(&at)) {
        probes_seen++;
        for (int i = 0; i < AUTOTUNE_SAMPLES; i++) {
            int32_t d = autotune_probe_freq(&at) - peak;

            if (d < 0) {
                d = -d;
            }
            /* Deterministic synthetic S-meter in dBm: closer to the peak is
             * stronger, i.e. a HIGHER (less negative) level. */
            autotune_feed(&at, (int16_t) (-60 - (d / 100)));
        }
        autotune_next(&at);
    }

    best = autotune_result(&at, AUTOTUNE_MIN_IMPROVE_DB);

    CHECK(probes_seen == 21);
    CHECK(best == peak);                  /* the scan finds the real peak */
    CHECK(at.start_freq == start);        /* the anchor is kept for restore */
}

void test_autotune(void) {
    test_score_median_odd();
    test_score_median_even();
    test_score_unsorted_and_empty();
    test_plan_zigzag();
    test_plan_window_not_multiple_of_step();
    test_plan_defaults_and_clamp();
    test_feed_completes_after_k_samples();
    test_next_walks_the_plan();
    test_result_picks_the_best_side();
    test_result_restores_when_start_wins();
    test_result_restores_when_improvement_is_below_margin();
    test_result_restores_without_measurements();
    test_result_is_deterministic_on_a_tie();
    test_scan_end_to_end();
}

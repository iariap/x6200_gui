/*
 * Unit tests: IARU band plan strip geometry (band_plan.c).
 *
 * band_plan_segments() is pure (no LVGL), so the pixel maths can be checked on
 * the host: which segments fall inside the displayed span, how they are clipped
 * and how they map to the 800 px strip.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <string.h>

#include "band_plan_data.h"
#include "test_framework.h"

#define SPAN_100K   50000       /* half width of a 100 kHz span */

/* The 20 m CW segment (14000-14070 kHz) with the dial at 14.074 MHz and a
 * 100 kHz span: 14000..14070 kHz maps to the left part of the strip. */
static void test_segments_20m_cw(void) {
    bp_visible_t v[BAND_PLAN_MAX];
    int          n = band_plan_segments(14074000, SPAN_100K, v, BAND_PLAN_MAX);

    /* Left edge of the span is 14024000: the CW segment is entirely left of it,
     * the digital segment (14070-14099) starts inside the span. */
    CHECK(n >= 1);

    bool saw_dig = false;

    for (int i = 0; i < n; i++) {
        if (v[i].type == BP_DIG && v[i].w > 0) {
            saw_dig = true;
            CHECK(v[i].x >= 0);
            CHECK(v[i].x + v[i].w <= BAND_PLAN_W);
        }
    }

    CHECK(saw_dig);
}

/* Everything inside the span: dial on the CW segment with a narrow span. */
static void test_segments_centred(void) {
    bp_visible_t v[BAND_PLAN_MAX];
    int          n = band_plan_segments(14040000, 10000, v, BAND_PLAN_MAX);

    CHECK(n == 1);
    CHECK(v[0].type == BP_CW);
    CHECK(v[0].x == 0);                       /* covers the whole span */
    CHECK(v[0].w == BAND_PLAN_W);
}

/* Wide span: several bands and segments at once, all inside the strip. */
static void test_segments_wide_span(void) {
    bp_visible_t v[BAND_PLAN_MAX];
    int          n = band_plan_segments(15000000, 5000000, v, BAND_PLAN_MAX);

    CHECK(n > 3);

    for (int i = 0; i < n; i++) {
        CHECK(v[i].w > 0);
        CHECK(v[i].x >= 0);
        CHECK(v[i].x + v[i].w <= BAND_PLAN_W);
    }

    /* Order follows the frequency plan, i.e. left to right on screen. */
    for (int i = 1; i < n; i++) {
        CHECK(v[i].x >= v[i - 1].x);
    }
}

/* Out of any band plan: nothing to show. */
static void test_segments_empty(void) {
    bp_visible_t v[BAND_PLAN_MAX];

    CHECK(band_plan_segments(12000000, 50000, v, BAND_PLAN_MAX) == 0);   /* 12 m gap */
    CHECK(band_plan_segments(1000000, 50000, v, BAND_PLAN_MAX) == 0);    /* below 160 m */
}

/* Degenerate inputs must not crash or write out of bounds. */
static void test_segments_edge_cases(void) {
    bp_visible_t v[BAND_PLAN_MAX];

    CHECK(band_plan_segments(14074000, 0, v, BAND_PLAN_MAX) == 0);
    CHECK(band_plan_segments(14074000, -100, v, BAND_PLAN_MAX) == 0);
    CHECK(band_plan_segments(14074000, SPAN_100K, NULL, BAND_PLAN_MAX) == 0);
    CHECK(band_plan_segments(14074000, SPAN_100K, v, 0) == 0);

    /* A tiny output buffer must truncate, not overflow. */
    int n = band_plan_segments(15000000, 5000000, v, 2);
    CHECK(n == 2);
}

/* A partially visible segment is clipped to the strip. */
static void test_segments_clipping(void) {
    bp_visible_t v[BAND_PLAN_MAX];
    /* Span 14065..14085 kHz: crosses the CW/DIG boundary at 14070 kHz, so the CW
     * segment is clipped on the left edge and the digital one on the right. */
    int n = band_plan_segments(14075000, 10000, v, BAND_PLAN_MAX);

    CHECK(n == 2);

    if (n == 2) {
        CHECK(v[0].type == BP_CW);
        CHECK(v[0].x == 0);
        CHECK(v[0].w < BAND_PLAN_W);

        CHECK(v[1].type == BP_DIG);
        CHECK(v[1].x == v[0].w);
        CHECK(v[1].x + v[1].w == BAND_PLAN_W);
    }
}

/* CB 27 MHz (issue #169): the four grids are four BP_CB segments between 12 m and
 * 10 m, so the strip is painted violet with a "CB" label. Values are exact with
 * the current geometry (BAND_PLAN_W = 800). */
static void test_segments_cb(void) {
    bp_visible_t v[BAND_PLAN_MAX];
    int          n;

    /* Span inside grid C: the whole strip, one BP_CB segment. */
    n = band_plan_segments(27185000, 12000, v, BAND_PLAN_MAX);
    CHECK(n == 1);
    CHECK(v[0].type == BP_CB);
    CHECK(v[0].x == 0);
    CHECK(v[0].w == BAND_PLAN_W);

    /* Grid A starts in the middle of the span: the segment is clipped on the left. */
    n = band_plan_segments(26065000, 12000, v, BAND_PLAN_MAX);
    CHECK(n == 1);
    CHECK(v[0].type == BP_CB);
    CHECK(v[0].x == 400);
    CHECK(v[0].w == 400);

    /* Span crossing the A/B gap (10 kHz): two BP_CB segments, one per grid. */
    n = band_plan_segments(26515000, 20000, v, BAND_PLAN_MAX);
    CHECK(n == 2);
    CHECK(v[0].type == BP_CB);
    CHECK(v[0].x == 0);
    CHECK(v[0].w == 200);
    CHECK(v[1].type == BP_CB);
    CHECK(v[1].x == 400);
    CHECK(v[1].w == 400);

    /* 6 kHz span inside the C/D gap: nothing is painted. */
    n = band_plan_segments(27410000, 3000, v, BAND_PLAN_MAX);
    CHECK(n == 0);

    /* Off-channel (27.000) but inside grid C: still one BP_CB segment. */
    n = band_plan_segments(27000000, 12000, v, BAND_PLAN_MAX);
    CHECK(n == 1);
    CHECK(v[0].type == BP_CB);
    CHECK(v[0].x == 0);
    CHECK(v[0].w == BAND_PLAN_W);

    /* Wide span over the whole 26-28 MHz region: several BP_CB segments in order. */
    n = band_plan_segments(26700000, 800000, v, BAND_PLAN_MAX);
    CHECK(n >= 3);

    for (int i = 0; i < n; i++) {
        CHECK(v[i].type == BP_CB);
        CHECK(v[i].w > 0);
        CHECK(v[i].x >= 0);
        CHECK(v[i].x + v[i].w <= BAND_PLAN_W);
    }

    for (int i = 1; i < n; i++) {
        CHECK(v[i].x >= v[i - 1].x);
    }
}

void test_band_plan(void) {
    test_segments_20m_cw();
    test_segments_centred();
    test_segments_wide_span();
    test_segments_empty();
    test_segments_edge_cases();
    test_segments_clipping();
    test_segments_cb();
}

/*
 * Unit tests: adaptive tuning (speed -> multiplier, timeout, decay and the
 * "Freq acceleration" gating added in the 2026-10-11 ROM revision).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>

#include "adaptive_tuning.h"
#include "test_framework.h"

void test_tick_set(uint32_t ms);

static void test_init(void) {
    adaptive_tuning_state_t st;

    test_tick_set(0);
    adaptive_tuning_init(&st);

    CHECK(st.active);
    CHECK(st.event_count == 0);
    CHECK_CLOSE(st.current_multiplier, 1.0f, 0.001f);
}

/* The multiplier must be governed by the user setting. */
static void test_gate(void) {
    CHECK_CLOSE(adaptive_multiplier_gate(50.0f, ADAPTIVE_ACCEL_NONE), 1.0f, 0.001f);
    CHECK_CLOSE(adaptive_multiplier_gate(50.0f, ADAPTIVE_ACCEL_LITE), 10.0f, 0.001f);
    CHECK_CLOSE(adaptive_multiplier_gate(50.0f, ADAPTIVE_ACCEL_STRONG), 50.0f, 0.001f);

    /* Values below the cap pass through... */
    CHECK_CLOSE(adaptive_multiplier_gate(2.0f, ADAPTIVE_ACCEL_LITE), 2.0f, 0.001f);
    CHECK_CLOSE(adaptive_multiplier_gate(1.0f, ADAPTIVE_ACCEL_NONE), 1.0f, 0.001f);

    /* ...and the fine-tuning floor is always 0.1x. */
    CHECK_CLOSE(adaptive_multiplier_gate(0.0f, ADAPTIVE_ACCEL_LITE), 0.1f, 0.001f);
    CHECK_CLOSE(adaptive_multiplier_gate(-5.0f, ADAPTIVE_ACCEL_STRONG), 0.1f, 0.001f);
}

/* A single click (or very slow rotation) must give ultra-fine tuning. */
static void test_single_event_is_ultra_fine(void) {
    adaptive_tuning_state_t st;

    test_tick_set(0);
    adaptive_tuning_init(&st);
    adaptive_tuning_add_event(&st, 1, 10);
    test_tick_set(10);
    adaptive_tuning_update(&st);

    CHECK_CLOSE(st.current_multiplier, 0.1f, 0.001f);
}

/* Slow rotation stays below 1x. */
static void test_slow_rotation_is_fine(void) {
    adaptive_tuning_state_t st;

    test_tick_set(0);
    adaptive_tuning_init(&st);

    for (int i = 0; i < 3; i++) {
        adaptive_tuning_add_event(&st, 1, 400 * (i + 1));
    }

    test_tick_set(1200);
    adaptive_tuning_update(&st);

    CHECK(st.current_multiplier < 1.0f);
    CHECK(st.current_multiplier >= 0.1f);
}

/* Fast rotation must accelerate (band scanning with STRONG). */
static void test_fast_rotation_accelerates(void) {
    adaptive_tuning_state_t st;

    test_tick_set(0);
    adaptive_tuning_init(&st);

    for (int i = 0; i < 10; i++) {
        adaptive_tuning_add_event(&st, 1, 10 * i);
    }

    test_tick_set(90);
    adaptive_tuning_update(&st);

    CHECK(st.current_multiplier >= 20.0f);
    CHECK(adaptive_multiplier_gate(st.current_multiplier, ADAPTIVE_ACCEL_LITE) <= 10.0f);
}

/* After 500 ms without events the multiplier returns to 1x. */
static void test_timeout_resets(void) {
    adaptive_tuning_state_t st;

    test_tick_set(0);
    adaptive_tuning_init(&st);

    for (int i = 0; i < 10; i++) {
        adaptive_tuning_add_event(&st, 1, 10 * i);
    }

    test_tick_set(90);
    adaptive_tuning_update(&st);
    CHECK(st.current_multiplier > 1.0f);

    test_tick_set(st.last_event_ms + EVENT_TIMEOUT_MS + 100);
    adaptive_tuning_update(&st);

    CHECK_CLOSE(st.current_multiplier, 1.0f, 0.001f);
    CHECK(st.event_count == 0);
}

/* Decay brings the multiplier back towards 1x when idle. */
static void test_decay(void) {
    adaptive_tuning_state_t st;

    test_tick_set(0);
    adaptive_tuning_init(&st);

    st.current_multiplier = 50.0f;
    st.last_event_ms = 0;

    test_tick_set(1000);
    adaptive_tuning_decay(&st);

    CHECK(st.current_multiplier < 50.0f);
    CHECK(st.current_multiplier > 1.0f);
}

/* The speed -> multiplier curve must be monotonic (predictable UX). */
static void test_speed_curve_is_monotonic(void) {
    float prev = -1.0f;
    bool  monotonic = true;

    for (float speed = 0.0f; speed <= 200.0f; speed += 0.5f) {
        float m = speed_to_multiplier(speed);

        if (m < prev - 0.001f) {
            monotonic = false;
            break;
        }

        prev = m;
    }

    CHECK(monotonic);
    CHECK_CLOSE(speed_to_multiplier(0.0f), 0.1f, 0.001f);
    CHECK_CLOSE(speed_to_multiplier(2.0f), 0.1f, 0.001f);
    CHECK_CLOSE(speed_to_multiplier(4.0f), 1.0f, 0.001f);
    CHECK_CLOSE(speed_to_multiplier(6.0f), 2.0f, 0.001f);
    CHECK_CLOSE(speed_to_multiplier(10.0f), 5.0f, 0.001f);
    CHECK_CLOSE(speed_to_multiplier(20.0f), 20.0f, 0.001f);
    CHECK_CLOSE(speed_to_multiplier(100.0f), 100.0f, 0.001f);
    CHECK_CLOSE(speed_to_multiplier(10000.0f), 100.0f, 0.001f);
}

void test_adaptive_tuning(void) {
    test_init();
    test_gate();
    test_single_event_is_ultra_fine();
    test_slow_rotation_is_fine();
    test_fast_rotation_accelerates();
    test_timeout_resets();
    test_decay();
    test_speed_curve_is_monotonic();
}

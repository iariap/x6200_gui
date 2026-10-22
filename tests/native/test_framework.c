/*
 * Assertion framework implementation, split out of test_main.c so other test
 * binaries (e.g. the WSPR round trip) can use it without pulling in the main().
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "test_framework.h"

int tests_run = 0;
int tests_failed = 0;

void test_check(bool ok, const char *expr, const char *file, int line) {
    tests_run++;

    if (!ok) {
        tests_failed++;
        printf("FALLO %s:%d: %s\n", file, line, expr);
    }
}

void test_check_close(float got, float want, float tol, const char *expr, const char *file, int line) {
    float diff = got - want;

    if (diff < 0.0f) {
        diff = -diff;
    }

    tests_run++;

    if (diff > tol) {
        tests_failed++;
        printf("FALLO %s:%d: %s (got %.4f, want %.4f +-%.4f)\n", file, line, expr, got, want, tol);
    }
}

void test_adaptive_tuning(void);
void test_audio_enhancement(void);
void test_ft8_udp(void);
void test_update_check(void);
void test_band_plan(void);
void test_avi_writer(void);
void test_wfview(void);

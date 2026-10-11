/*
 * Runner for the native/ARM unit tests (see docs/TESTING.md).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

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

int main(void) {
    test_adaptive_tuning();
    test_audio_enhancement();
    test_ft8_udp();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

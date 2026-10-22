/*
 * Runner for the Auto Tune v2 sign test (tests/native/test_cw_tune_sign.c). It is
 * a separate binary because it links liquid-dsp (the same FFT src/cw.cpp runs):
 * the shared native suite must keep building on a host without liquid.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_cw_tune_sign(void);

int main(void) {
    test_cw_tune_sign();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

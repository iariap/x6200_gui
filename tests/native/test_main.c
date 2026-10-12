/*
 * Runner for the native/ARM unit tests (see docs/TESTING.md).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"




void test_adaptive_tuning(void);
void test_audio_enhancement(void);
void test_ft8_udp(void);
void test_update_check(void);
void test_band_plan(void);
void test_avi_writer(void);
void test_wfview(void);

int main(void) {
    test_adaptive_tuning();
    test_audio_enhancement();
    test_ft8_udp();
    test_update_check();
    test_band_plan();
    test_avi_writer();
    test_wfview();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

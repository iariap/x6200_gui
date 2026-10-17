/*
 * Runner for the multipass dialog test (tests/native/test_swrscan_multipass.c).
 *
 * This binary is compiled with SWRSCAN_MULTIPASS_TX_ENABLED=1 (see
 * tests/native/run_native_tests.sh): it is the only place where the 3/5-pass
 * path of the REAL dialog runs, and it does so against the owner's fixtures with
 * the radio stubbed. The shipping binary keeps the gate at 0 and refuses the
 * multipass TX (tests/native/test_swrscan.c, test_swrscan_multipass_gate).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_swrscan_mp_single_valley(void);
void test_swrscan_mp_outliers_5pass(void);
void test_swrscan_mp_zoom_spans(void);
void test_swrscan_mp_double_and_flat(void);
void test_swrscan_mp_moving_5pass(void);
void test_swrscan_mp_stop_partial(void);
void test_swrscan_mp_passes_comparison(void);
void test_swrscan_mp_instrumentation(void);

int main(void) {
    test_swrscan_mp_single_valley();
    test_swrscan_mp_outliers_5pass();
    test_swrscan_mp_zoom_spans();
    test_swrscan_mp_double_and_flat();
    test_swrscan_mp_moving_5pass();
    test_swrscan_mp_stop_partial();
    test_swrscan_mp_passes_comparison();
    test_swrscan_mp_instrumentation();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

/*
 * Runner for the native SWR Scan dialog test (tests/native/test_swrscan.c).
 * It is a separate binary because the real src/dialog_swrscan.c is compiled
 * against its own stub tree (tests/native/stubs_swrscan/), not the shared stubs
 * of the main native suite.
 *
 * This is the DEFAULT build: the multipass TX gate is off (SWRSCAN_MULTIPASS_TX_ENABLED
 * = 0), so the dialog-level multipass replay lives in its own binary
 * (tests/native/test_swrscan_multipass.c + swrscan_multipass_main.c).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_swrscan(void);
void test_swrscan_sweep(void);
void test_swrscan_render(void);
void test_swrscan_sweep_end(void);
void test_swrscan_multipass_gate(void);
void test_swrscan_readout(void);
void test_swrscan_timeout(void);

int main(void) {
    test_swrscan();
    test_swrscan_sweep();
    test_swrscan_render();
    test_swrscan_sweep_end();
    test_swrscan_multipass_gate();
    test_swrscan_readout();
    test_swrscan_timeout();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

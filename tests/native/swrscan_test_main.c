/*
 * Runner for the native SWR Scan dialog test (tests/native/test_swrscan.c).
 * It is a separate binary because the real src/dialog_swrscan.c is compiled
 * against its own stub tree (tests/native/stubs_swrscan/), not the shared stubs
 * of the main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_swrscan(void);

int main(void) {
    test_swrscan();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

/*
 * Runner for the native clock test (tests/native/test_clock.c). It is a
 * separate binary because src/clock.c is compiled against its own stub tree
 * (tests/native/stubs_clock/), not the shared stubs of the main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_clock(void);

int main(void) {
    test_clock();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

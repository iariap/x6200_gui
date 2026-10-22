/*
 * Runner for the native spectrum test (tests/native/test_spectrum.c). It is a
 * separate binary because src/spectrum.c is compiled against its own stub tree
 * (tests/native/stubs_spectrum/), not the shared stubs of the main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_spectrum(void);

int main(void) {
    test_spectrum();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

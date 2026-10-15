/*
 * Runner for the native digital modes test (tests/native/test_digital_modes.c).
 * It is a separate binary because the real src/cfg/digital_modes.c is compiled
 * against its own stub tree (tests/native/stubs_digital/), not the shared stubs
 * of the main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_digital_modes(void);

int main(void) {
    test_digital_modes();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

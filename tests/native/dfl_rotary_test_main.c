/*
 * Runner for the native DFL rotary engine test (tests/native/test_dfl_rotary.c).
 * Separate binary because the engine (src/dfl_rotary.c) is compiled against its
 * own cfg/msg stubs (tests/native/stubs_dfl_rotary).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_dfl_rotary(void);

int main(void) {
    test_dfl_rotary();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

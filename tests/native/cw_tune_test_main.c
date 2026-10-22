/*
 * Runner for the native cw_tune_ui test (tests/native/test_cw_tune.c). It is a
 * separate binary because src/cw_tune_ui.c is compiled against its own stub tree
 * (tests/native/stubs_cw_tune/), not the shared stubs of the main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_cw_tune(void);

int main(void) {
    test_cw_tune();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

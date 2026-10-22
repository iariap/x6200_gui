/*
 * Runner for the native hkey test (tests/native/test_hkey.c). It is a separate
 * binary because src/hkey.c is compiled against its own stub tree
 * (tests/native/stubs_hkey/), not the shared stubs of the main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_hkey(void);

int main(void) {
    test_hkey();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

/*
 * Runner for the native events-queue test (tests/native/test_events.c). It is a
 * separate binary because src/events.c is compiled against its own stub tree
 * (tests/native/stubs_events/), not the shared stubs of the main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_events(void);

int main(void) {
    test_events();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

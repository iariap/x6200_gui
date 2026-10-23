/*
 * Runner for the radio_thread test (tests/native/test_radio_thread.c).
 *
 * Separate binary: src/radio.c is compiled against its own stub tree
 * (tests/native/stubs_radio/), not the shared stubs of the main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_radio_thread(void);

int main(void) {
    test_radio_thread();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

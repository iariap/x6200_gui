/*
 * Runner for the native remote_server test (tests/native/test_remote.cpp).
 * A separate binary because the real src/remote_server.cpp is compiled against its
 * own stub tree (tests/native/stubs_remote/), not the shared stubs of the main suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_remote(void);

int main(void) {
    test_remote();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

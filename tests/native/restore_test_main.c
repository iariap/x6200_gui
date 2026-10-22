/*
 * Runner for the native dialog restore test (tests/native/test_dialog_restore.c).
 * It is a separate binary because the real src/dialog.c is compiled against its
 * own stub tree (tests/native/stubs_restore/), not the shared stubs of the main
 * native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_dialog_restore(void);

int main(void) {
    test_dialog_restore();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

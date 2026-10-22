/*
 * Runner for the native DFL editor model test (tests/native/test_dfl_editor.c).
 * Separate binary because the model is compiled against its own control.h stub
 * (tests/native/stubs_dfl).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_dfl_editor(void);

int main(void) {
    test_dfl_editor();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

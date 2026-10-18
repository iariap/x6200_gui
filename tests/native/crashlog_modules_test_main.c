/*
 * Runner for the native CRASHLOG module-table test (issue #131).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_crashlog_modules(void);

int main(void) {
    test_crashlog_modules();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

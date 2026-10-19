/*
 * Runner for the native CB-bands migration test (issue #146).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_migrations_cb(void);

int main(void) {
    test_migrations_cb();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

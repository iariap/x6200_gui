/*
 * Runner del test nativo del squelch (issue #166). Binario propio porque
 * src/sql_policy.c se compila contra el stub de la lib del control
 * (tests/native/stubs_sql/), no contra la lib real.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_sql_policy(void);

int main(void) {
    test_sql_policy();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

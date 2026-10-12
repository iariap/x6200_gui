/* Entry point for the standalone WSPR test binary. SPDX-License-Identifier: LGPL-2.1-or-later */

#include <stdio.h>

#include "test_framework.h"

void test_wspr(void);

int main(void) {
    test_wspr();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

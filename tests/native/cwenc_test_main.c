/*
 * Runner for the native CW encoder test (tests/native/test_cw_encoder.c).
 * It is a separate binary because the real src/cw_encoder.c is compiled against
 * its own stub tree (tests/native/stubs_cwenc/), not the shared stubs of the
 * main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_cw_encoder_null(void);
void test_cw_encoder_send(void);

int main(void) {
    test_cw_encoder_null();
    test_cw_encoder_send();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

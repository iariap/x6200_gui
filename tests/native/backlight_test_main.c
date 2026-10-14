/*
 * Runner for the native backlight test (tests/native/test_backlight.c). It is a
 * separate binary because the real src/backlight.c is compiled against its own
 * stub tree (tests/native/stubs_backlight/), not the shared stubs of the main
 * native suite: the shared ones define backlight_set_brightness(), which
 * backlight.c defines itself.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_backlight(void);

int main(void) {
    test_backlight();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

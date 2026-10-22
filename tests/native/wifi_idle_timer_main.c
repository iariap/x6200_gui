/*
 * Runner for the native WiFi idle-pump test (tests/native/test_wifi_idle_timer.cpp).
 * A separate binary because the real src/wifi.cpp is compiled against its own stub tree
 * (tests/native/stubs_wifi/) plus the timer/pump fixture of tests/native/wifi_idle_stubs.cpp,
 * not against the shared stubs of the main suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_wifi_idle_timer(void);

int main(void) {
    test_wifi_idle_timer();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

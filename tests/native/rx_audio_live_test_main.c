/*
 *  Runner de los tests host del router RX live (#364).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_rx_audio_live(void);

int main(void) {
    test_rx_audio_live();
    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}

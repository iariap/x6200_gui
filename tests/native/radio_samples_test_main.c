/*
 * Runner del test host de conversion de muestras del flujo RX (issue #377).
 * Binario separado porque src/radio_samples.h es un header C puro que no
 * necesita el arbol de stubs del firmware (ver tests/native/test_radio_samples.c).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_radio_samples(void);

int main(void) {
    test_radio_samples();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

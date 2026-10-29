/*
 * Runner del test del callback de captura (issue #351, parte 2). Binario propio porque
 * src/audio.c se compila por inclusion contra los stubs de PulseAudio/ALSA
 * (tests/native/stubs_audio/), que no linkean con el stage principal.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_audio_callback(void);

int main(void) {
    test_audio_callback();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

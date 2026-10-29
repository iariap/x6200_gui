/*
 * Params stub for the capture-callback stage (issue #351, parte 2).
 *
 * src/audio.c solo toca params.hmic y params.imic (el restore de audio_play_en, que el
 * test no ejercita: ahi tiene que dar cero llamadas) y las tres escrituras de control de
 * la radio, que el stub cuenta. El resto de params_t no existe a proposito: este stage no
 * compila params.c ni el SDK de la radio.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t hmic;
    uint8_t imic;
} params_t;

extern params_t params;

/* Escrituras de ruteo/control de la radio: el callback de captura no puede llamarlas. */
void x6200_control_hmic_set(uint8_t value);
void x6200_control_imic_set(uint8_t value);
void x6200_control_record_set(bool on);

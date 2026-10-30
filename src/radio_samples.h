/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Conversion de las muestras de RX del flujo del BASE (issue #377).
 *
 *  En X6200Control v0.1.1 el payload del flujo declara
 *
 *      uint8_t samples[512];
 *
 *  o sea 512 BYTES de muestra, no 512 float. La GUI vendor los convierte a
 *  float restando el offset 0x96 (150) a cada muestra; ese es el unico escalado
 *  que usa el equipo y el que replica este header. El contrato de ABI
 *  (sizeof/offsetof, header real con el que se compila) se verifica con
 *  _Static_assert en src/radio.c: si el SDK no es el v0.1.1 esperado, el build
 *  se detiene con BLOCKED_GUI_SAMPLE_ABI en vez de reinterpretar floats.
 *
 *  Este header es C puro y no depende del SDK ni de LVGL a proposito: la
 *  conversion se puede testear en host (tests/native/test_radio_samples.c)
 *  sin el arbol del firmware.
 */

#pragma once

#include <stdint.h>

/* Cantidad de muestras del pack v0.1.1 (uint8_t samples[512]) y que consume
 * dsp_samples() como RADIO_SAMPLES floats. */
#define RADIO_FLOW_SAMPLES      (512)

/* Offset de la APP vendor: cada byte es una muestra centrada en 150 (0x96). */
#define RADIO_FLOW_SAMPLE_BIAS  (150)

/* Una muestra del pack -> la muestra de audio que espera el DSP. */
static inline float radio_flow_sample_to_float(uint8_t raw) {
    return (float) ((int) raw - RADIO_FLOW_SAMPLE_BIAS);
}

/* Convierte `count` muestras u8 a float. `out` tiene que tener capacidad para
 * al menos `count` floats; el llamador real pasa RADIO_FLOW_SAMPLES. */
static inline void radio_flow_samples_to_float(float *out, const uint8_t *raw, uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        out[i] = radio_flow_sample_to_float(raw[i]);
    }
}

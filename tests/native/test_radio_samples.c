/*
 * Native test de la conversion de muestras del flujo RX (issue #377).
 *
 * x6200_flow_t de X6200Control v0.1.1 trae `uint8_t samples[512]`: 512 BYTES en
 * dB (formato de flujo BASE 1.0.7/1.0.8), no 512 floats. La GUI los convierte
 * con `(float)((int)byte - 126)` (commit upstream c74df4a3) antes de pasarlos a
 * dsp_samples(), que los trata como dB y los convierte a potencia. Este test
 * fija el contrato de conversion y el borde de las 512 muestras:
 *
 *   - puntos deterministas: 0 -> -126.0f, 126 -> 0.0f, 255 -> 129.0f;
 *   - las 512 muestras se convierten una por una, en orden y sin escalado
 *     adicional;
 *   - buffers de tamano EXACTO (una lectura/escritura fuera de rango aborta con
 *     ASan/UBSan) y canarios en los bordes para el build sin sanitizadores;
 *   - un patron de bytes que leido como float seria 1.0f no se reinterpreta:
 *     cada byte se convierte por separado (regresion del bug del cast).
 *
 * El header src/radio_samples.h es C puro (sin SDK ni LVGL) justamente para
 * poder compilar este test en host. El ABI real del pack (sizeof/offsetof) no se
 * prueba aca: lo fijan los _Static_assert de src/radio.c contra el header del
 * SDK con el que se compila la GUI (STOP BLOCKED_GUI_SAMPLE_ABI si no es v0.1.1).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>
#include <stdlib.h>

#include "test_framework.h"

#include "radio_samples.h"

/* Puntos deterministas del contrato 1.0.7/1.0.8. */
static void test_puntos_del_contrato(void) {
    CHECK_CLOSE(radio_flow_sample_to_float(0), -126.0f, 0.0f);
    CHECK_CLOSE(radio_flow_sample_to_float(125), -1.0f, 0.0f);
    CHECK_CLOSE(radio_flow_sample_to_float(126), 0.0f, 0.0f);
    CHECK_CLOSE(radio_flow_sample_to_float(0x7e), 0.0f, 0.0f);   /* 126 == 0x7e */
    CHECK_CLOSE(radio_flow_sample_to_float(255), 129.0f, 0.0f);
}

/* Las 512 muestras del pack completo, exactas y en orden. */
static void test_512_muestras(void) {
    uint8_t raw[RADIO_FLOW_SAMPLES];
    float   out[RADIO_FLOW_SAMPLES];

    for (int i = 0; i < RADIO_FLOW_SAMPLES; i++) {
        raw[i] = (uint8_t) i;   /* i > 255 vuelve a 0..255: el pack son bytes */
    }

    radio_flow_samples_to_float(out, raw, RADIO_FLOW_SAMPLES);

    for (int i = 0; i < RADIO_FLOW_SAMPLES; i++) {
        CHECK_CLOSE(out[i], (float) ((int) raw[i] - RADIO_FLOW_SAMPLE_BIAS), 0.0f);
    }

    /* Los bordes del pack: primera muestra (byte 0) y ultima (byte 255). */
    CHECK_CLOSE(out[0], -126.0f, 0.0f);
    CHECK_CLOSE(out[RADIO_FLOW_SAMPLES - 1], 129.0f, 0.0f);
}

/* Borde de las 512 muestras: nada fuera de rango, ni leido ni escrito. */
static void test_bordes_sin_oob(void) {
    static const uint8_t IN_FIRST  = 0xA5;
    static const uint8_t IN_LAST   = 0x5A;
    static const float   OUT_FIRST = 12345.0f;
    static const float   OUT_LAST  = -54321.0f;

    /* Asignacion del tamano exacto: ASan/UBSan marcan cualquier acceso de mas. */
    uint8_t *raw = malloc((size_t) RADIO_FLOW_SAMPLES + 2);
    float   *out = malloc(((size_t) RADIO_FLOW_SAMPLES + 2) * sizeof(float));

    CHECK(raw != NULL && out != NULL);

    if (raw == NULL || out == NULL) {
        free(raw);
        free(out);
        return;
    }

    raw[0]                     = IN_FIRST;
    raw[RADIO_FLOW_SAMPLES + 1] = IN_LAST;
    for (int i = 0; i < RADIO_FLOW_SAMPLES; i++) {
        raw[1 + i] = (uint8_t) i;
    }

    out[0]                      = OUT_FIRST;
    out[RADIO_FLOW_SAMPLES + 1] = OUT_LAST;

    radio_flow_samples_to_float(out + 1, raw + 1, RADIO_FLOW_SAMPLES);

    /* Canarios: la conversion no toca los bytes de guarda. */
    CHECK(raw[0] == IN_FIRST);
    CHECK(raw[RADIO_FLOW_SAMPLES + 1] == IN_LAST);
    CHECK(out[0] == OUT_FIRST);
    CHECK(out[RADIO_FLOW_SAMPLES + 1] == OUT_LAST);

    /* Y las 512 posiciones convertidas son las del ejemplo del issue. */
    CHECK_CLOSE(out[1], -126.0f, 0.0f);
    CHECK_CLOSE(out[1 + 126], 0.0f, 0.0f);       /* byte 126 -> 0.0f */
    CHECK_CLOSE(out[RADIO_FLOW_SAMPLES], 129.0f, 0.0f);   /* byte 255 -> 129.0f */

    /* Constantes del contrato que el header expone. */
    CHECK(RADIO_FLOW_SAMPLES == 512);
    CHECK(RADIO_FLOW_SAMPLE_BIAS == 126);

    free(raw);
    free(out);
}

/* Regresion del bug: el patron de bytes de 1.0f little-endian no es una muestra
 * float; cada byte se convierte por separado con el offset. */
static void test_no_reinterpreta_float(void) {
    const uint8_t raw[4] = { 0x00, 0x00, 0x80, 0x3F };   /* 1.0f si se leyera como float */
    float         out[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    radio_flow_samples_to_float(out, raw, 4);

    CHECK_CLOSE(out[0], -126.0f, 0.0f);
    CHECK_CLOSE(out[1], -126.0f, 0.0f);
    CHECK_CLOSE(out[2], (float) (0x80 - RADIO_FLOW_SAMPLE_BIAS), 0.0f);
    CHECK_CLOSE(out[3], (float) (0x3F - RADIO_FLOW_SAMPLE_BIAS), 0.0f);
}

void test_radio_samples(void) {
    test_puntos_del_contrato();
    test_512_muestras();
    test_bordes_sin_oob();
    test_no_reinterpreta_float();
}

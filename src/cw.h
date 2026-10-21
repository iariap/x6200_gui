/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>

#include "helpers.h"
#include "autotune.h"


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <liquid/liquid.h>

void cw_init();

/* The keying speed in WPM. The X6100 reports the speed it is actually sending;
 * this GUI keeps the one set in the configuration, which is what the CW
 * keyboard app shows. */
int cw_get_wpm(void);

/* The CW trainer app decodes what the radio is keying: with this on, the CW DSP
 * runs even when the decoder setting is off. */
void cw_trainer_set(bool on);

void cw_put_audio_samples(unsigned int n, cfloat *samples);
void cw_put_audio_int_samples(unsigned int n, int16_t *samples);

/* --- Auto Tune v2 (issue #190): instantanea del ultimo resultado de FFT ----
 *
 * El decodificador CW ya calcula, una vez por trama de FFT de 128 muestras
 * (~185 ms), el offset del tono recibido respecto del centro del DDS
 * (`update_peak_freq`) y la relacion marca/ruido de esa misma trama
 * (`update_thresholds`). Auto Tune v2 necesita eso mismo, pero con edad,
 * secuencia y validez: el widget de sintonia (src/cw_tune_ui.c) no las tiene (su
 * estado es visual y coalescido) y por eso NO se usa como fuente de medida.
 *
 * El tipo de la instantanea es UNO SOLO: `autotune_sample_t` (src/autotune.h),
 * el mismo que consume el nucleo de Auto Tune. No hay copia ni conversion de
 * tipos: el contrato de la trama es el mismo en el DSP, en el nucleo y en la UI.
 *
 * `error_hz` va NORMALIZADO en el hilo del DSP: > 0 significa tono recibido por
 * encima del pitch objetivo (`cfg.key_tone`), en CW y en CWR. La FFT entrega el
 * negativo de ese corrimiento (docs/HALLAZGOS.md 52) y el centro del DDS puede
 * estar desplazado por el AUTO del decodificador (#139), asi que la conversion
 * se hace UNA sola vez, aca, compensando `rx_tone_offset` de la misma trama. El
 * nucleo de Auto Tune (src/autotune.c) nunca ve una coordenada cruda de la FFT. */

/* Copia coherente de la ultima muestra publicada (no bloquea: la region critica
 * es una copia de estructura). Devuelve false si el DSP todavia no publico
 * ninguna. Solo publica el hilo del DSP, una vez por trama procesada. */
bool cw_get_tune_sample(autotune_sample_t *out);

/* Marca que Auto Tune acaba de escribir el dial: el hilo del DSP invalida la
 * primera ventana posterior (puede traer audio de la frecuencia anterior). No
 * toca buffers compartidos: es una bandera atomica que consume el DSP. */
void cw_tune_note_dial_write(void);

/* --- conversion pura del pico de la FFT (la que usa src/cw.cpp) ------------
 *
 * Exponen la MISMA cuenta que corre en el lazo de audio, para que la prueba del
 * signo con senal sintetica (tests/native/test_cw_tune_sign.c) mida la
 * conversion real y no una copia. */

/* Bin del argmax -> frecuencia en Hz, con el orden de la FFT ya corregido
 * ("Fix fft order" de cw.cpp). */
static inline float cw_peak_freq_from_bin(int32_t bin, int32_t fft, float rate_hz) {
    return (((float) (fft - (bin + fft / 2) % fft) / (float) fft) - 0.5f) * rate_hz;
}

/* Frecuencia del pico -> error del tono respecto del PITCH configurado, ya
 * compensado el corrimiento del DDS de esa misma trama: el pico vale
 * key_tone + rx_tone_offset - A, asi que A - key_tone = rx_tone_offset - pico.
 * El signo normalizado (> 0 = tono por encima del pitch) se fija ACA, una sola
 * vez (docs/HALLAZGOS.md 52). */
static inline float cw_tune_error_hz(float peak_freq_hz, float rx_tone_offset_hz) {
    return rx_tone_offset_hz - peak_freq_hz;
}


// bool cw_change_decoder(int16_t df);
// float cw_change_snr(int16_t df);
// float cw_change_peak_beta(int16_t df);
// float cw_change_noise_beta(int16_t df);

#ifdef __cplusplus
}
#endif

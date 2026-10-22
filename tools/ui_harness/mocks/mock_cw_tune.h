/*
 *  Synthetic CW tone source for Auto Tune v2 (issue #190) in the host harness.
 *
 *  The bank has no DSP: src/cw.cpp (the FFT that publishes `cw_get_tune_sample`)
 *  is not compiled here, so the fixtures that need a CW tone get a TEST-ONLY
 *  injection of the snapshot instead of a fabricated RSSI or a faked production
 *  path. The injected sample carries the same fields the radio's DSP fills
 *  (monotonic sequence, virtual-clock timestamp, normalized error, mark/noise
 *  ratio, DDS offset, validity), so the assistant core and the whole LVGL
 *  session run exactly as on the radio.
 *
 *  Default state: NO signal (valid = false), which is what the bank can honestly
 *  claim without arming anything. A fixture arms a tone explicitly.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Vuelve al estado por defecto (sin senal) y reinicia secuencia y contadores. */
void mock_cw_tune_reset(void);

/* Arma un tono sintetico: `error_hz` es el error NORMALIZADO que publicaria el
 * DSP (> 0 = tono por encima del pitch) y `ratio` la relacion marca/ruido. */
void mock_cw_tune_set_tone(float error_hz, float ratio);

/* Sin marca detectada: ninguna trama es valida (default). */
void mock_cw_tune_set_no_signal(void);

/* Corrimiento del DDS vigente (AUTO del decodificador, #139). */
void mock_cw_tune_set_offset(float offset_hz);

/* Cuantas tramas se publicaron (para codificar que el fixture si las pidio). */
uint64_t mock_cw_tune_frames(void);

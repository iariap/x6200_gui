/*
 *  TEST-ONLY spectrum source for Auto Tune multimode (issue #205) in the host
 *  harness.
 *
 *  The bank has no DSP (src/dsp.cpp is not compiled), so the fixtures that need a
 *  carrier (AM/SAM) or a voice mass (USB/LSB) arm one explicitly instead of
 *  fabricating an S-meter reading or faking production code. What the fixture
 *  declares is the PHYSICAL situation at the dial of that moment; the injected
 *  snapshot (mocks/mock_dsp.c) recalculates the offset as the dial moves, the
 *  same way the receiver does, so a fixture can codify "one correction centers
 *  the carrier" or "the dial does not move before confirmation" as facts.
 *
 *  The generic noise floor is a fixture knob too: the policies' thresholds are
 *  relative to it, so a fixture that wants a weak signal moves the floor instead
 *  of inventing an absolute dB scale.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MOCK_SPECTRUM_DEFAULT_NOISE_DB (-100.0f)

/* Vuelve al estado por defecto (piso de ruido plano = sin senal) y reinicia
 * secuencia, epoch y contadores. */
void mock_spectrum_reset(void);

/* Piso de ruido del espectro sintetico (dB). */
void mock_spectrum_set_noise(float db);

/* Portadora angosta (AM/SAM) en `offset_hz` (con signo, respecto del dial
 * actual) con `level_db`. */
void mock_spectrum_set_carrier(float offset_hz, float level_db);

/* Masa de voz (USB/LSB) centrada en `center_hz` con `width_hz` de ancho y
 * `level_db`. */
void mock_spectrum_set_voice(float center_hz, float width_hz, float level_db);

/* Cuantas publicaciones se entregaron (para codificar que el fixture si las
 * pidio) y el epoch vigente. */
uint64_t mock_spectrum_frames(void);
uint64_t mock_spectrum_epoch(void);

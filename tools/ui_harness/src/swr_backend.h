/*
 *  Fake ROE acquisition backend for the host UI harness (issue #125, section 7).
 *
 *  The SWR Scan window transmits while it measures, so the host cannot run it
 *  against a radio: this backend is the deterministic stand-in for the radio's
 *  side of the contract. It replays one of the owner's ready-made datasets
 *  (tests/fixtures/swrscan/swr_mock_samples.csv) through the REAL dialog
 *  (src/dialog_swrscan.c) on the harness' virtual clock:
 *
 *    - the grid (centre and span) is derived from the dataset itself and pushed
 *      into cfg.swrscan_span, so the frequencies the dialog asks for are exactly
 *      the dataset's own, point by point;
 *    - one "flow packet" per point at the real cadence (SWRSCAN_DWELL_MS), with
 *      the radio thread's tick (dialog_swrscan_poll) after each one, so the stall
 *      watchdog runs inside the sweep;
 *    - the instrument is mocks/mock_radio.c: `radio_start_swrscan` only counts,
 *      `radio_set_freq` only records, and the binary links no RF library. There
 *      is no code path from a fixture to a transmission.
 *
 *  What it refuses (fail-closed, never a green capture of something else): a
 *  dataset that does not exist, a fixture whose dial does not match the
 *  dataset's centre, a session the dialog's own gate rejects, and a finished
 *  session that a later packet still moves.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "scenario.h"

/* Carga el dataset que pide el fixture y deja la grilla del dialogo en la del
 * dataset. Devuelve false (y explica por que) si el fixture no se puede
 * reproducir tal cual. */
bool swr_backend_prepare(const scenario_t *scenario);

/* Pulsa Run (y Stop si el fixture lo pide) y corre el barrido sobre el reloj
 * virtual: un paquete del flow por punto. Termina con el estado que el fixture
 * pidio capturar (a mitad de barrido o al final, congelado). */
void swr_backend_run(const scenario_t *scenario);

/* Bloque JSON con la traza de la corrida (dataset, puntos, comandos, TX, estado
 * y la prueba de congelamiento). No emite nada si no hubo backend. */
void swr_backend_json(FILE *f, const scenario_t *scenario);

/* true si esta captura corrio con el backend (la pantalla `swrscan`). El JSON
 * del arnes solo agrega el bloque `swr` cuando lo hubo. */
bool swr_backend_used(void);

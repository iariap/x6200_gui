/*
 * Interface entre el test de presets del DFL (tests/native/test_dfl_presets.c) y
 * sus stubs (tests/native/dfl_presets_stubs.c): Subjects en memoria, el espejo
 * de los setters de src/cfg/mode.c y los contadores de escrituras prohibidas.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

/* Arma el estado vivo y limpia los contadores. */
void dfl_pt_reset(int32_t mode, int32_t low, int32_t high);

int32_t dfl_pt_low(void);
int32_t dfl_pt_high(void);
int32_t dfl_pt_bw(void);

/* Escrituras DIRIGIDAS a cfg_cur.filter.bw (deben ser 0: el BW lo derivan los
 * observers de src/cfg/mode.c a partir de los limites, no el preset). */
int dfl_pt_bw_writes(void);

/* Escrituras a cfg_cur.filter.real.from/to (deben ser 0: solo update_real_filters
 * los toca, y no esta en este test). */
int dfl_pt_real_writes(void);

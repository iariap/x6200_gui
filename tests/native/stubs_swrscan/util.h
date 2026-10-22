/*
 * Minimal util stub for the native SWR Scan dialog test (issues #85 and #125).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

void split_freq(int32_t freq, uint16_t *mhz, uint16_t *khz, uint16_t *hz);

/* Reloj virtual del test (swrscan_stubs.c): el barrido mide el timeout de RF con get_time(),
 * asi que el test controla el tiempo sin sleeps reales. */
uint64_t get_time(void);

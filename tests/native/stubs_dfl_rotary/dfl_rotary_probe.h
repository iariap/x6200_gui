/*
 * Sonda del test del motor de la rueda del editor DFL: el estado del Subject
 * falso y el ultimo mensaje emitido, para afirmar que el valor aplicado sale del
 * Subject (nunca de un valor optimista).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

/* Estado inicial del Subject falso (modo, low, high, bw). */
void dfl_test_reset(int32_t mode, int32_t low, int32_t high, int32_t bw);

int32_t dfl_test_low(void);
int32_t dfl_test_high(void);
int32_t dfl_test_bw(void);

/* Ultimo texto que el motor paso a msg_update_text_fmt(). */
const char *dfl_test_last_msg(void);

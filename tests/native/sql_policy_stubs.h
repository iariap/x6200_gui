/*
 * Registro de llamadas de los stubs del control para el test del squelch
 * (issue #166). Ver tests/native/sql_policy_stubs.c.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* --- registro -------------------------------------------------------------- */

void     sql_stub_reset(void);

unsigned sql_stub_sql_calls(void);
unsigned sql_stub_sql_fm_calls(void);
unsigned sql_stub_enable_calls(void);

uint8_t  sql_stub_last_sql(void);
uint8_t  sql_stub_last_sql_fm(void);
bool     sql_stub_last_enable(void);

/* Secuencia de llamadas en orden: 'v' = sql_set (voz), 'f' = sql_fm_set,
 * 'E' = enable(true), 'e' = enable(false). Permite afirmar el ORDEN
 * (nivel antes que enable) y que no haya llamadas de mas. */
const char *sql_stub_sequence(void);

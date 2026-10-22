/*
 * Stubs capturadores de la lib del control para el test del squelch (issue #166).
 *
 * Cada setter del squelch registra la llamada y su valor, y ademas deja una
 * letra en una secuencia: asi el test puede afirmar no solo "se llamo", sino el
 * ORDEN (el nivel antes del enable) y que no hubo llamadas de mas por cambio.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "aether_radio/x6200_control/control.h"
#include "sql_policy_stubs.h"

#include <string.h>

#define SQL_STUB_SEQ_MAX 64

static struct {
    unsigned sql_calls;
    unsigned sql_fm_calls;
    unsigned enable_calls;

    uint8_t  last_sql;
    uint8_t  last_sql_fm;
    bool     last_enable;

    size_t   seq_len;
    char     seq[SQL_STUB_SEQ_MAX];
} rec;

static void seq_push(char c) {
    if (rec.seq_len + 1 < SQL_STUB_SEQ_MAX) {
        rec.seq[rec.seq_len++] = c;
        rec.seq[rec.seq_len] = '\0';
    }
}

void sql_stub_reset(void) {
    memset(&rec, 0, sizeof(rec));
}

unsigned sql_stub_sql_calls(void)    { return rec.sql_calls; }
unsigned sql_stub_sql_fm_calls(void) { return rec.sql_fm_calls; }
unsigned sql_stub_enable_calls(void) { return rec.enable_calls; }

uint8_t  sql_stub_last_sql(void)     { return rec.last_sql; }
uint8_t  sql_stub_last_sql_fm(void)  { return rec.last_sql_fm; }
bool     sql_stub_last_enable(void)  { return rec.last_enable; }

const char *sql_stub_sequence(void)  { return rec.seq; }

void x6200_control_sql_set(uint8_t sql) {
    rec.sql_calls++;
    rec.last_sql = sql;
    seq_push('v');
}

void x6200_control_sql_fm_set(uint8_t sql) {
    rec.sql_fm_calls++;
    rec.last_sql_fm = sql;
    seq_push('f');
}

void x6200_control_sql_enable_set(bool on) {
    rec.enable_calls++;
    rec.last_enable = on;
    seq_push(on ? 'E' : 'e');
}

/*
 * Stub implementations for the native digital_modes test
 * (tests/native/test_digital_modes.c).
 *
 * They stand in for the sqlite3, cfg and LVGL symbols src/cfg/digital_modes.c
 * calls and expose a single hook, digital_modes_stub_set_row(), so the test can
 * drive both branches of cfg_digital_load(): a row (SQLITE_ROW, the successful
 * load) and no row (SQLITE_DONE, the load that leaves the label unset).
 *
 * The fake does not model SQL semantics (WHERE/ORDER BY/ABS): cfg_digital_load()
 * is blind to the query text -- it binds :type/:freq, steps once and reads
 * columns 0..2 -- and the invariant under test (the label getter) does not
 * depend on which row a real SELECT would have picked.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sqlite3.h"
#include "cfg.h"

struct sqlite3      { int unused; };
struct sqlite3_stmt { int unused; };

static struct sqlite3_stmt stmt_stub;

/* What the next sqlite3_step() returns, and the row it exposes. */
static int          step_result  = SQLITE_DONE;
static const char * column_label = NULL;
static int          column_freq  = 0;
static int          column_mode  = 0;

void digital_modes_stub_set_row(bool has_row, const char * label, int32_t freq, int32_t mode) {
    step_result  = has_row ? SQLITE_ROW : SQLITE_DONE;
    column_label = label;
    column_freq  = (int) freq;
    column_mode  = (int) mode;
}

int sqlite3_prepare_v2(sqlite3 * db, const char * zSql, int nByte, sqlite3_stmt ** ppStmt, const char ** pzTail) {
    (void) db;
    (void) zSql;
    (void) nByte;
    (void) pzTail;
    *ppStmt = &stmt_stub;
    return SQLITE_OK;
}

int sqlite3_bind_int(sqlite3_stmt * stmt, int index, int value) {
    (void) stmt;
    (void) index;
    (void) value;
    return SQLITE_OK;
}

int sqlite3_bind_parameter_index(sqlite3_stmt * stmt, const char * name) {
    (void) stmt;
    return (name != NULL && strcmp(name, ":type") == 0) ? 1 : 2;
}

int sqlite3_step(sqlite3_stmt * stmt) {
    (void) stmt;
    return step_result;
}

int sqlite3_column_int(sqlite3_stmt * stmt, int col) {
    (void) stmt;
    return (col == 1) ? column_freq : column_mode;
}

const unsigned char * sqlite3_column_text(sqlite3_stmt * stmt, int col) {
    (void) stmt;
    (void) col;
    return (const unsigned char *) column_label;
}

int sqlite3_reset(sqlite3_stmt * stmt) {
    (void) stmt;
    return SQLITE_OK;
}

int sqlite3_clear_bindings(sqlite3_stmt * stmt) {
    (void) stmt;
    return SQLITE_OK;
}

const char * sqlite3_errmsg(sqlite3 * db) {
    (void) db;
    return "stub";
}

/* ---- cfg surface ---------------------------------------------------------- */

struct Subject { int32_t value; };

static Subject fg_freq_subject;
static Subject mode_subject;

cfg_cur_t cfg_cur = { &fg_freq_subject, &mode_subject };

int32_t subject_get_int(Subject * subj) {
    return subj->value;
}

void subject_set_int(Subject * subj, int32_t val) {
    subj->value = val;
}

/* ---- accessors used by the test ------------------------------------------- */

int32_t digital_modes_stub_fg_freq(void) {
    return fg_freq_subject.value;
}

int32_t digital_modes_stub_mode(void) {
    return mode_subject.value;
}

/* ---- LVGL log surface ----------------------------------------------------- */

void lv_stub_log(const char * level, const char * fmt, ...) {
    va_list args;

    fprintf(stderr, "[%s] ", level);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

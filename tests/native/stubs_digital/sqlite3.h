/*
 * Minimal sqlite3 surface for the native digital_modes test
 * (tests/native/test_digital_modes.c). Only what src/cfg/digital_modes.c
 * touches, declared with the same signatures (and the same result codes) as the
 * pinned SQLite 3 header, so the staged module compiles unchanged under
 * -Wall -Wextra.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef struct sqlite3      sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

#define SQLITE_OK   0   /* successful result */
#define SQLITE_ROW  100 /* sqlite3_step() has another row ready */
#define SQLITE_DONE 101 /* sqlite3_step() has finished executing */

int sqlite3_prepare_v2(sqlite3 * db, const char * zSql, int nByte, sqlite3_stmt ** ppStmt, const char ** pzTail);
int sqlite3_bind_int(sqlite3_stmt * stmt, int index, int value);
int sqlite3_bind_parameter_index(sqlite3_stmt * stmt, const char * name);
int sqlite3_step(sqlite3_stmt * stmt);
int sqlite3_column_int(sqlite3_stmt * stmt, int col);

const unsigned char * sqlite3_column_text(sqlite3_stmt * stmt, int col);

int sqlite3_reset(sqlite3_stmt * stmt);
int sqlite3_clear_bindings(sqlite3_stmt * stmt);
const char * sqlite3_errmsg(sqlite3 * db);

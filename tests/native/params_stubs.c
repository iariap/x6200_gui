/*
 * Stubs de la base y de los servicios externos para probar src/params/params.c
 * en el host (issue #186, seccion 6: Settings y persistencia).
 *
 * La prueba abre una base SQLite REAL en un archivo temporal y ejercita el
 * camino real params_init() -> params_load() -> saneo -> params_save():
 *   - sin clave, T queda en 7;
 *   - 3/7/15 guardados se recuperan identicos;
 *   - 0/2/16/255 se reemplazan por 7 y quedan dirty para persistir.
 *
 * Lo unico simulado es el resto del firmware: cfg_init(), la voz, el log de
 * arranque y el historial de mensajes CW. No hay red ni hardware.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "lvgl/lvgl.h"

#include "params/params.h"
#include "params/db.h"
#include "cfg/cfg.h"
#include "dialog_msg_cw.h"
#include "voice.h"
#include "util.h"

sqlite3 *db = NULL;

bool database_init(void) {
    const char *path = getenv("PARAMS_TEST_DB");

    if (path == NULL || *path == 0) {
        return false;
    }

    if (sqlite3_open(path, &db) != SQLITE_OK) {
        return false;
    }

    return sql_query_exec(
        "CREATE TABLE IF NOT EXISTS params(name TEXT PRIMARY KEY, val);"
        "CREATE TABLE IF NOT EXISTS mode_params(mode INTEGER, name TEXT, val);"
        "CREATE TABLE IF NOT EXISTS bands(id INTEGER, name TEXT, start_freq INTEGER,"
        " stop_freq INTEGER, type INTEGER);");
}

bool sql_query_exec(const char *sql) {
    return db != NULL && sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK;
}

static void write_value(const char *name, const char *sql_bind, bool *dirty, int kind, const void *data) {
    sqlite3_stmt *stmt = NULL;

    if (db == NULL || name == NULL) {
        return;
    }

    if (sqlite3_prepare_v2(db, sql_bind, -1, &stmt, 0) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    switch (kind) {
        case 0:
            sqlite3_bind_int(stmt, 2, *(const int *) data);
            break;
        case 1:
            sqlite3_bind_int64(stmt, 2, (sqlite3_int64) *(const uint64_t *) data);
            break;
        case 2:
            sqlite3_bind_double(stmt, 2, *(const double *) data);
            break;
        default:
            sqlite3_bind_text(stmt, 2, (const char *) data, -1, SQLITE_STATIC);
            break;
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (dirty != NULL) {
        *dirty = false;
    }
}

void params_write_int(const char *name, int data, bool *dirty) {
    write_value(name, "INSERT INTO params(name,val) VALUES(?,?) "
                      "ON CONFLICT(name) DO UPDATE SET val=excluded.val", dirty, 0, &data);
}

void params_write_int64(const char *name, uint64_t data, bool *dirty) {
    write_value(name, "INSERT INTO params(name,val) VALUES(?,?) "
                      "ON CONFLICT(name) DO UPDATE SET val=excluded.val", dirty, 1, &data);
}

void params_write_float(const char *name, float data, bool *dirty) {
    double value = data;

    write_value(name, "INSERT INTO params(name,val) VALUES(?,?) "
                      "ON CONFLICT(name) DO UPDATE SET val=excluded.val", dirty, 2, &value);
}

void params_write_text(const char *name, const char *data, bool *dirty) {
    write_value(name, "INSERT INTO params(name,val) VALUES(?,?) "
                      "ON CONFLICT(name) DO UPDATE SET val=excluded.val", dirty, 3, data);
}

int cfg_init(sqlite3 *handle) {
    (void) handle;
    return 0;
}

void dialog_msg_cw_append(uint32_t id, const char *val) {
    (void) id;
    (void) val;
}

void voice_say_bool(const char *prompt, bool x) {
    (void) prompt;
    (void) x;
}

void voice_say_int(const char *prompt, int32_t x) {
    (void) prompt;
    (void) x;
}

void boot_mark(const char *etapa) {
    (void) etapa;
}

uint64_t get_time(void) {
    return 1;   /* no nulo: params_unlock() arma el temporizador de guardado */
}

void _lv_log_add(lv_log_level_t level, const char *file, int line,
                 const char *func, const char *format, ...) {
    (void) level;
    (void) file;
    (void) line;
    (void) func;
    (void) format;
}

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Persistencia SQLite de los presets del DFL (issue #183; ver
 *  src/dfl_presets_store.h).
 *
 *  Nada de SQL desde callbacks de dibujo: el modelo carga todo en cache al
 *  inicializar (dfl_presets_init) y solo entra aca al guardar (accion explicita
 *  del operador, en el hilo de GUI).
 *
 *  Las tablas las crea la migracion _5 (src/params/migrations.c) y, para una
 *  instalacion nueva, sql/params.sql. Si la tabla no existe, las funciones
 *  fallan con un mensaje veraz y no tocan nada mas de la base.
 */

#include "dfl_presets_store.h"

#include "params/db.h"

#include <stdio.h>
#include <string.h>

static char last_error[160] = "";

const char *dfl_store_last_error(void) {
    return last_error;
}

static void set_error(const char *what, const char *detail) {
    snprintf(last_error, sizeof(last_error), "%s: %s", what, detail ? detail : "?");
}

bool dfl_store_available(void) {
    return db != NULL;
}

/* --- Lecturas -------------------------------------------------------------- */

bool dfl_store_load_memory(int family, int slot, int32_t *low, int32_t *high) {
    sqlite3_stmt *stmt = NULL;
    bool          found = false;

    if (db == NULL) {
        set_error("sin base", "params.db no esta abierta");
        return false;
    }

    int rc = sqlite3_prepare_v2(db,
                                "SELECT low_hz, high_hz FROM dfl_filter_memories"
                                " WHERE family=? AND memory_slot=?",
                                -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        set_error("memorias", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_int(stmt, 1, family);
    sqlite3_bind_int(stmt, 2, slot);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *low   = sqlite3_column_int(stmt, 0);
        *high  = sqlite3_column_int(stmt, 1);
        found  = true;
    }

    sqlite3_finalize(stmt);
    last_error[0] = '\0';
    return found;
}

bool dfl_store_load_favorite(int family, int slot, int32_t *source) {
    sqlite3_stmt *stmt = NULL;
    bool          found = false;

    if (db == NULL) {
        set_error("sin base", "params.db no esta abierta");
        return false;
    }

    int rc = sqlite3_prepare_v2(db,
                                "SELECT source FROM dfl_filter_favorites"
                                " WHERE family=? AND quick_slot=?",
                                -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        set_error("favoritos", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_int(stmt, 1, family);
    sqlite3_bind_int(stmt, 2, slot);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *source = sqlite3_column_int(stmt, 0);
        found   = true;
    }

    sqlite3_finalize(stmt);
    last_error[0] = '\0';
    return found;
}

/* --- Escrituras ------------------------------------------------------------ */

/* BEGIN IMMEDIATE + INSERT OR REPLACE + COMMIT, con ROLLBACK si algo falla: la
 * fila entra o no entra, nunca a medias. `n` es la cantidad de parametros
 * enteros que liga la sentencia (3 para favoritos, 4 para memorias). */
static bool write_row(const char *insert_sql, const int *vals, int n) {
    sqlite3_stmt *stmt = NULL;
    bool          ok   = false;

    if (db == NULL) {
        set_error("sin base", "params.db no esta abierta");
        return false;
    }

    if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        set_error("transaccion", sqlite3_errmsg(db));
        return false;
    }

    int rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        set_error("guardado", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return false;
    }

    for (int i = 0; i < n; i++) {
        sqlite3_bind_int(stmt, i + 1, vals[i]);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        set_error("guardado", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return false;
    }

    if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        set_error("commit", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return false;
    }

    last_error[0] = '\0';
    ok = true;
    return ok;
}

bool dfl_store_save_memory(int family, int slot, int32_t low, int32_t high) {
    const int vals[4] = { family, slot, low, high };

    return write_row("INSERT OR REPLACE INTO dfl_filter_memories(family, memory_slot, low_hz, high_hz)"
                     " VALUES(?,?,?,?)",
                     vals, 4);
}

bool dfl_store_save_favorite(int family, int slot, int32_t source) {
    const int vals[3] = { family, slot, source };

    return write_row("INSERT OR REPLACE INTO dfl_filter_favorites(family, quick_slot, source)"
                     " VALUES(?,?,?)",
                     vals, 3);
}

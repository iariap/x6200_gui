/*
 * Persistencia real de freq_accel_threshold (issue #186, seccion 6).
 *
 * Cada ejecucion abre una base SQLite propia, siembra (o no) la clave y llama
 * al params_init() REAL de src/params/params.c. El script la invoca una vez por
 * caso porque params_init() arranca el hilo de guardado del firmware:
 *   missing       sin clave                       -> 7
 *   stored-3/7/15 clave guardada                  -> mismo valor
 *   invalid-0/2/16/255 clave fuera de rango       -> 7 y dirty
 *   set           3 guardado, se cambia a 15      -> 15 en la base
 *   reload        15 guardado                     -> 15 (reinicio)
 *
 * Variables: PARAMS_TEST_DB, PARAMS_TEST_SEED (opcional), PARAMS_TEST_EXPECT,
 * PARAMS_TEST_DIRTY (0/1), PARAMS_TEST_SET (opcional), PARAMS_TEST_EXPECT_DB
 * (opcional).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "params/params.h"

static int failures = 0;

static void check(bool condition, const char *what) {
    if (condition) {
        return;
    }

    printf("    FALLO: %s\n", what);
    failures++;
}

static bool env_int(const char *name, int *out) {
    const char *text = getenv(name);

    if (text == NULL || *text == 0) {
        return false;
    }

    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (end == text || *end != 0) {
        return false;
    }

    *out = (int) value;
    return true;
}

static void seed_database(const char *path, int seed) {
    sqlite3 *handle = NULL;

    if (sqlite3_open(path, &handle) != SQLITE_OK) {
        printf("    FALLO: no se pudo abrir %s\n", path);
        failures++;
        return;
    }

    sqlite3_exec(handle,
        "CREATE TABLE IF NOT EXISTS params(name TEXT PRIMARY KEY, val);"
        "CREATE TABLE IF NOT EXISTS mode_params(mode INTEGER, name TEXT, val);"
        "CREATE TABLE IF NOT EXISTS bands(id INTEGER, name TEXT, start_freq INTEGER,"
        " stop_freq INTEGER, type INTEGER);",
        NULL, NULL, NULL);

    if (seed >= 0) {
        char sql[160];

        snprintf(sql, sizeof(sql),
                 "INSERT OR REPLACE INTO params(name,val) VALUES('freq_accel_threshold',%d)", seed);
        sqlite3_exec(handle, sql, NULL, NULL, NULL);
    }

    sqlite3_close(handle);
}

static int read_database(const char *path) {
    sqlite3      *handle = NULL;
    sqlite3_stmt *stmt = NULL;
    int          value = -1;

    if (sqlite3_open(path, &handle) != SQLITE_OK) {
        return -1;
    }

    if (sqlite3_prepare_v2(handle, "SELECT val FROM params WHERE name='freq_accel_threshold'",
                           -1, &stmt, 0) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) {
        value = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(handle);

    return value;
}

int main(void) {
    const char *path = getenv("PARAMS_TEST_DB");
    int        seed = -1;
    int        expected = -1;
    int        expect_dirty = 0;
    int        set_value = -1;
    int        expect_db = -1;

    if (path == NULL || *path == 0) {
        printf("PASS: params_threshold (sin PARAMS_TEST_DB, nada que probar)\n");
        return 0;
    }

    bool has_seed = env_int("PARAMS_TEST_SEED", &seed);

    if (!env_int("PARAMS_TEST_EXPECT", &expected)) {
        printf("FALLO: falta PARAMS_TEST_EXPECT\n");
        return 1;
    }

    (void) env_int("PARAMS_TEST_DIRTY", &expect_dirty);
    bool has_set = env_int("PARAMS_TEST_SET", &set_value);
    (void) env_int("PARAMS_TEST_EXPECT_DB", &expect_db);

    seed_database(path, has_seed ? seed : -1);

    params_init();

    /* get_time() devuelve un valor fijo, asi que params_ready_to_save() nunca
     * dispara: el hilo de guardado no compite con estas comprobaciones. No se
     * toma params_mux aca porque params_uint8_set() lo toma y se bloquearia. */
    check(params.freq_accel_threshold.x == expected, "valor cargado");
    check(params.freq_accel_threshold.min == 3, "minimo 3");
    check(params.freq_accel_threshold.max == 15, "maximo 15");
    check(params.freq_accel_threshold.dirty == (expect_dirty != 0), "dirty esperado");

    if (has_set) {
        params_uint8_set(&params.freq_accel_threshold, (uint8_t) set_value);
        check(params.freq_accel_threshold.x == (uint8_t) set_value, "valor aplicado");
        check(params.freq_accel_threshold.dirty, "dirty tras params_uint8_set");
        params_flush();
        check(!params.freq_accel_threshold.dirty, "dirty limpio tras params_flush");
    }

    if (expect_db >= 0) {
        check(read_database(path) == expect_db, "valor persistido en la base");
    }

    if (failures == 0) {
        printf("PASS: params_threshold\n");
    } else {
        printf("FAIL: params_threshold (%d fallos)\n", failures);
    }

    /* El hilo del firmware queda bloqueado en params_mux: salir sin esperarlo. */
    fflush(stdout);
    _Exit(failures == 0 ? 0 : 1);
}

/*
 * Migracion de bandas CB (_4_cb_bands, src/params/migrations.c, issue #146).
 *
 * Corre contra una base sqlite3 real EN MEMORIA, con la misma src/params/
 * migrations.c que se flashea: es la unica forma de fijar la tabla, porque la
 * migracion es la que arregla las radios que actualizan en el lugar (la base
 * nueva ya trae los rangos corregidos por sql/bands_cb.csv). El patron es el de
 * los otros tests nativos: se copia el modulo real junto a stubs minimos de sus
 * headers y se compila con sqlite3 del host.
 *
 * Casos:
 *   - los cuatro rangos quedan en los grids canonicos de 40 canales;
 *   - una frecuencia guardada por DEBAJO del rango nuevo se recorta al canal 1
 *     (el caso real de la ROM: 201 tenia vfoa_freq = 26,055,000);
 *   - una frecuencia guardada por ARRIBA del rango nuevo tambien se recorta;
 *   - una frecuencia que ya cae en el rango nuevo se conserva;
 *   - no se toca ninguna otra banda ni otro parametro (vfoa_mode);
 *   - la version sube a 4 y correrla de nuevo (idempotente) deja todo igual.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <sqlite3.h>

#include "db.h"
#include "migrations.h"
#include "test_framework.h"

static int exec(const char *sql) {
    return sqlite3_exec(db, sql, NULL, NULL, NULL);
}

/* Primer entero de una consulta de una sola fila; -1 si no hay fila o falla. */
static int query_int(const char *sql) {
    sqlite3_stmt *stmt;
    int           val = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        return -1;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        val = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return val;
}

/* Base vieja: los rangos de 230-240 kHz de la ROM y la version en 3, para que la
 * unica migracion que corra sea la 4. */
static void seed_old_rom(void) {
    CHECK(exec("CREATE TABLE IF NOT EXISTS bands(id INTEGER PRIMARY KEY, name TEXT, start_freq INTEGER,"
               " stop_freq INTEGER, type INTEGER);"
               "CREATE TABLE IF NOT EXISTS band_params(bands_id INTEGER, name TEXT, val INTEGER,"
               " UNIQUE(bands_id, name) ON CONFLICT REPLACE);"
               "CREATE TABLE IF NOT EXISTS version(id INT NOT NULL DEFAULT 0);"
               "DELETE FROM bands; DELETE FROM band_params; DELETE FROM version;"
               "INSERT INTO bands VALUES"
               " (201,'CB A',26055000,26295000,1),"
               " (202,'CB B',26515000,26745000,1),"
               " (203,'CB C',26965000,27195000,1),"
               " (204,'CB D',27415000,27645000,1);"
               "INSERT INTO band_params VALUES"
               /* caso real de la ROM: por debajo del inicio nuevo (26,065,000) */
               " (201,'vfoa_freq',26055000),"
               " (201,'vfoa_mode',6),"
               /* ya cae en el rango nuevo: se conserva */
               " (201,'vfob_freq',26295000),"
               " (202,'vfoa_freq',26515000),"
               " (203,'vfoa_freq',27185000),"
               /* por arriba del fin nuevo de D (27,855,000) */
               " (204,'vfob_freq',27900000),"
               /* otra banda: no se toca */
               " (300,'vfoa_freq',144300000);"
               "INSERT INTO version(id) VALUES(3);") == SQLITE_OK);
}

/* Los cuatro grids canonicos (seccion 4.2 del issue). */
static void check_canonical_ranges(void) {
    CHECK(query_int("SELECT start_freq FROM bands WHERE id=201") == 26065000);
    CHECK(query_int("SELECT stop_freq  FROM bands WHERE id=201") == 26505000);
    CHECK(query_int("SELECT start_freq FROM bands WHERE id=202") == 26515000);
    CHECK(query_int("SELECT stop_freq  FROM bands WHERE id=202") == 26955000);
    CHECK(query_int("SELECT start_freq FROM bands WHERE id=203") == 26965000);
    CHECK(query_int("SELECT stop_freq  FROM bands WHERE id=203") == 27405000);
    CHECK(query_int("SELECT start_freq FROM bands WHERE id=204") == 27415000);
    CHECK(query_int("SELECT stop_freq  FROM bands WHERE id=204") == 27855000);
}

/* Frecuencias guardadas: clamp al canal 1 por abajo/arriba y respeto de lo que
 * ya cae en el rango nuevo. */
static void check_saved_freqs(void) {
    CHECK(query_int("SELECT val FROM band_params WHERE bands_id=201 AND name='vfoa_freq'") == 26065000);
    CHECK(query_int("SELECT val FROM band_params WHERE bands_id=201 AND name='vfob_freq'") == 26295000);
    CHECK(query_int("SELECT val FROM band_params WHERE bands_id=202 AND name='vfoa_freq'") == 26515000);
    CHECK(query_int("SELECT val FROM band_params WHERE bands_id=203 AND name='vfoa_freq'") == 27185000);
    CHECK(query_int("SELECT val FROM band_params WHERE bands_id=204 AND name='vfob_freq'") == 27415000);
    /* otras filas intactas */
    CHECK(query_int("SELECT val FROM band_params WHERE bands_id=201 AND name='vfoa_mode'") == 6);
    CHECK(query_int("SELECT val FROM band_params WHERE bands_id=300 AND name='vfoa_freq'") == 144300000);
}

void test_migrations_cb(void) {
    CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);

    seed_old_rom();
    CHECK(migrations_apply() == 0);
    CHECK(query_int("SELECT id FROM version") == 4);
    check_canonical_ranges();
    check_saved_freqs();

    /* Idempotente: volver a la version 3 y correrla de nuevo deja lo mismo. */
    CHECK(exec("DELETE FROM version; INSERT INTO version(id) VALUES(3);") == SQLITE_OK);
    CHECK(exec("UPDATE bands SET stop_freq=27645000 WHERE id=204;") == SQLITE_OK);  /* ensucia a proposito */
    CHECK(migrations_apply() == 0);
    CHECK(query_int("SELECT id FROM version") == 4);
    check_canonical_ranges();
    check_saved_freqs();

    /* Ya migrada (version 4): no vuelve a correr ni toca nada. */
    CHECK(exec("UPDATE bands SET start_freq=1 WHERE id=201;") == SQLITE_OK);
    CHECK(migrations_apply() == 0);
    CHECK(query_int("SELECT id FROM version") == 4);
    CHECK(query_int("SELECT start_freq FROM bands WHERE id=201") == 1);

    sqlite3_close(db);
    db = NULL;
}

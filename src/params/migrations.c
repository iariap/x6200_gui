/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2024 Georgy Dyuldin aka R2RFE
 */

#define _GNU_SOURCE

#include "migrations.h"

#include "db.h"
#include "../cfg/digital_modes.h"
#include "../theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZEOF_ARRAY(arr) (sizeof(arr) > 0 ? sizeof(arr) / sizeof(arr[0]) : 0)

static int get_current_version(int * ver);
static int set_current_version(int ver);

/* Migrations functions */
static int _0_init_migrations() {
    int rc;
    rc = sqlite3_exec(db, "INSERT INTO version(id) VALUES(0)", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Cannot insert 0 version: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

static int _1_create_ftx_table() {
    int rc;
    char *query;
    /* SIN BEGIN/COMMIT propios: migrations_apply() envuelve cada migracion en
     * una transaccion y actualiza `version` dentro de la MISMA, asi que o queda
     * todo o no queda nada (issue #213, punto 4). */
    rc = asprintf(&query,
        "CREATE TABLE IF NOT EXISTS digital_modes("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "label varchar(64) NOT NULL, "
            "freq INTEGER NOT NULL CHECK(freq > 0), "
            "mode INTEGER NOT NULL DEFAULT 3 CHECK(mode >= 0 AND mode <= 7), "
            "type INTEGER NOT NULL, "
            "CONSTRAINT freq_type_uniq UNIQUE(freq, type) "
        ");"
        "CREATE INDEX IF NOT EXISTS digital_modes_type_idx ON digital_modes (type);"
        "CREATE INDEX IF NOT EXISTS digital_modes_freq_idx ON digital_modes (freq);"
        "INSERT INTO digital_modes(label, freq, mode, type) "
        "SELECT m1.val, m2.val, m3.val, %u  "
        "FROM memory AS m1  "
        "INNER JOIN memory AS m2 ON m1.id == m2.id  "
        "INNER JOIN memory AS m3 ON m1.id == m3.id  "
        "WHERE m1.name='label' AND m2.name='vfoa_freq' AND m3.name = 'vfoa_mode' AND m1.id >= 100 AND m1.id < 200; "
        "INSERT INTO digital_modes(label, freq, mode, type) "
        "SELECT m1.val, m2.val, m3.val, %u  "
        "FROM memory AS m1  "
        "INNER JOIN memory AS m2 ON m1.id == m2.id  "
        "INNER JOIN memory AS m3 ON m1.id == m3.id  "
        "WHERE m1.name='label' AND m2.name='vfoa_freq' AND m3.name = 'vfoa_mode' AND m1.id >= 200;",
        CFG_DIG_TYPE_FT8, CFG_DIG_TYPE_FT4
    );
    if (rc == -1) {
        printf("Cannot allocate SQL query\n");
        return 1;
    }
    rc = sqlite3_exec(db, query, NULL, NULL, NULL);
    free(query);
    if (rc != SQLITE_OK) {
        printf("Cannot migrate: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

/* 2: frecuencias del ATU de indice a Hz (formato viejo).
 *
 * La transformacion NO se puede repetir: freq*50000+25000 manda los indices
 * bajos (0..9) a valores que tambien son < 500000, asi que correrla dos veces
 * multiplica otra vez datos que YA estan en Hz. Como no se puede saber por fila
 * cual es cual, la guarda mira el conjunto: si no hay ninguna fila en el rango
 * de indice no hay nada que convertir (idempotente); si hay filas en los dos
 * rangos la procedencia es ambigua y se ABORTA con error explicito antes de
 * mutar nada -- preservar la base vale mas que convertir a ciegas. */
static int _2_update_atu_freq() {
    sqlite3_stmt *stmt;
    int           indices = 0;
    int           in_hz   = 0;
    int           rc;

    rc = sqlite3_prepare_v2(db,
        "SELECT coalesce(sum(freq < 500000), 0), coalesce(sum(freq >= 500000), 0) FROM atu",
        -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        printf("Cannot read atu frequencies: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        indices = sqlite3_column_int(stmt, 0);
        in_hz   = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (indices == 0) {
        /* atu vacia o ya en Hz: no hay nada que convertir. */
        return 0;
    }
    if (in_hz != 0) {
        printf("atu has %d rows in Hz and %d in the old index form: ambiguous provenance, "
               "refusing to convert\n", in_hz, indices);
        return 1;
    }

    rc = sqlite3_exec(db, "UPDATE atu SET freq=freq * 50000 + 25000 WHERE freq < 500000", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Cannot update ATU frequencies: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

/* Retira los temas legados y migra el valor guardado (issue #74).
 *
 * El catalogo de temas es la coleccion de seis archivos (docs/THEMES.md): el selector ya no ofrece
 * IU1WXY/Classic/Outdoor/Outdoor-Negative/Legacy/VISION, ni los archivos Holo/Material 3/RS, ni un
 * tema propio de DATA. Un `theme_name` guardado que no sea uno de los seis deja de ser elegible,
 * asi que migra al default (Aurora): el arranque no carga un tema retirado ni cae a un fallback
 * silencioso. NO se toca ningun archivo: los temas DATA del usuario quedan en /mnt/themes (el
 * epic #66 exige "temas DATA no destruidos"), solo dejan de ser el tema activo.
 *
 * Idempotente: un valor ya valido (o ya migrado) no se reescribe; correr la migracion dos veces
 * deja lo mismo. Es un UPDATE de una fila (params.name es PRIMARY KEY); no toca otras settings. */
static int _3_migrate_theme_name() {
    sqlite3_stmt *stmt;
    int           rc;
    bool          retire = false;

    rc = sqlite3_prepare_v2(db, "SELECT val FROM params WHERE name='theme_name'", -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        printf("Cannot read theme_name: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *val = (const char *) sqlite3_column_text(stmt, 0);

        /* Un valor vacio no es un tema: el codigo aplica su default (Aurora). Un valor que no sea
         * uno de los seis, en cambio, hay que retirarlo. */
        retire = val != NULL && *val != '\0' && !theme_offered(val);
    }
    sqlite3_finalize(stmt);

    if (!retire) {
        return 0;
    }
    rc = sqlite3_exec(db, "UPDATE params SET val='" THEME_DEFAULT "' WHERE name='theme_name'", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Cannot migrate theme_name: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

/* 4: bandas CB a los grids canonicos de 40 canales (issue #146).
 *
 * Las filas de `bands_cb.csv` de las ROM viejas traian tramos de 230-240 kHz
 * (24 posiciones), no los grids de 40 canales (440 kHz): el plan 26,965-27,405
 * quedaba casi todo afuera de la banda C y `cfg_band_set_freq_for_vfo()` guardaba
 * parametros bajo pk = -1. Aca se corrigen los cuatro rangos y se recorta a la
 * base del grid la frecuencia guardada que quedo fuera del rango nuevo (no se
 * borra ninguna fila: solo se clampan vfoa_freq/vfob_freq).
 *
 * Idempotente: un valor ya migrado no se reescribe, y una base nueva (que ya
 * trae los rangos corregidos por los CSV) no cambia. No toca ninguna otra tabla.
 * Propios en la seccion 8 del issue. */
static int _4_cb_bands(void) {
    static const char *sql =
        "UPDATE bands SET start_freq=26065000, stop_freq=26505000 WHERE id=201;"
        "UPDATE bands SET start_freq=26515000, stop_freq=26955000 WHERE id=202;"
        "UPDATE bands SET start_freq=26965000, stop_freq=27405000 WHERE id=203;"
        "UPDATE bands SET start_freq=27415000, stop_freq=27855000 WHERE id=204;"
        /* frecuencias guardadas que quedaron fuera del rango nuevo -> canal 1 */
        "UPDATE band_params SET val = (SELECT start_freq FROM bands WHERE id = band_params.bands_id) "
        " WHERE bands_id BETWEEN 201 AND 204 AND name IN ('vfoa_freq','vfob_freq') "
        "   AND (val < (SELECT start_freq FROM bands WHERE id = band_params.bands_id) "
        "     OR val > (SELECT stop_freq  FROM bands WHERE id = band_params.bands_id));";
    int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) { printf("Cannot migrate CB bands: %s\n", sqlite3_errmsg(db)); return 1; }
    return 0;
}

/* 5: memorias y favoritos de filtro del DFL (issue #183).
 *
 * Dos tablas NUEVAS y separadas: no se reutiliza `memory` (memorias de
 * frecuencia) ni `mode_params` (filter_low/high por modo), y no se renumera
 * press_action_t. `dfl_filter_memories` guarda los dos pares LOW/HIGH por
 * familia (1=SSB, 2=DIGI, 3=CW, 4=AM) y `dfl_filter_favorites` el ID de la
 * fuente (1..3 preset de fabrica, 4=Mem 1, 5=Mem 2) enlazada a cada uno de los
 * tres botones rapidos; el favorito guarda la REFERENCIA, no una copia.
 *
 * Idempotente: CREATE TABLE IF NOT EXISTS. Reejecutarla no borra memorias ni
 * favoritos, y una base nueva (que ya las trae por sql/params.sql) no cambia.
 * No toca ninguna tabla existente ni `version` mas alla del numero de version.
 * Los defaults "sin filas" (favorito n -> preset n) NO se escriben: los resuelve
 * src/dfl_presets.c en memoria. */
static int _5_dfl_filter_tables(void) {
    static const char *sql =
        "CREATE TABLE IF NOT EXISTS dfl_filter_memories ("
            "family INTEGER NOT NULL CHECK(family BETWEEN 1 AND 4),"
            "memory_slot INTEGER NOT NULL CHECK(memory_slot BETWEEN 1 AND 2),"
            "low_hz INTEGER NOT NULL CHECK(low_hz BETWEEN -10000 AND 9999),"
            "high_hz INTEGER NOT NULL CHECK(high_hz BETWEEN 1 AND 10000),"
            "PRIMARY KEY(family, memory_slot),"
            "CHECK(high_hz > low_hz));"
        "CREATE TABLE IF NOT EXISTS dfl_filter_favorites ("
            "family INTEGER NOT NULL CHECK(family BETWEEN 1 AND 4),"
            "quick_slot INTEGER NOT NULL CHECK(quick_slot BETWEEN 1 AND 3),"
            "source INTEGER NOT NULL CHECK(source BETWEEN 1 AND 5),"
            "PRIMARY KEY(family, quick_slot));";
    int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) { printf("Cannot create DFL filter tables: %s\n", sqlite3_errmsg(db)); return 1; }
    return 0;
}

/* Migrations array */
static int (*migrations[])() = {
    _0_init_migrations,
    _1_create_ftx_table,
    _2_update_atu_freq,
    _3_migrate_theme_name,
    _4_cb_bands,
    _5_dfl_filter_tables,
};

/* Migration count = the highest known version + 1. */
#define MIGRATIONS_COUNT SIZEOF_ARRAY(migrations)

/* Cada corrida de migraciones va en UNA transaccion, con la actualizacion de
 * `version` adentro: o queda TODA la secuencia con su version, o la base queda
 * exactamente como estaba (ROLLBACK). Esto importa porque las migraciones no son
 * todas idempotentes (la 2 no lo es: convierte indices a Hz) y una version a
 * medias dejaria una base que no arranca ni se puede reintentar limpio
 * (issue #213, punto 4). Nunca se sube el numero de version si algo falla. */
static int migration_begin(void) {
    if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        printf("Cannot begin migration transaction: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

static int migration_commit(void) {
    if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        printf("Cannot commit migration transaction: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

static void migration_rollback(void) {
    if (sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL) != SQLITE_OK) {
        printf("Cannot roll back migration transaction: %s\n", sqlite3_errmsg(db));
    }
}

int migrations_apply(void) {
    int ver;

    if (db == NULL) {
        printf("Database is not opened\n");
        return 1;
    }

    if (get_current_version(&ver) != 0) {
        printf("Cannot get current version\n");
        return 1;
    }
    if (ver > (int) MIGRATIONS_COUNT - 1) {
        /* Una base que dice ser mas nueva que este firmware se deja intacta: no
         * se migra hacia atras ni se sigue con un esquema desconocido. */
        printf("Database version %d is newer than this firmware (%zu): refusing to migrate\n",
               ver, MIGRATIONS_COUNT - 1);
        return 1;
    }
    if ((size_t) ver + 1 >= MIGRATIONS_COUNT) {
        return 0;   /* ya al dia: ni se abre una transaccion */
    }

    if (migration_begin() != 0) {
        return 1;
    }

    for (size_t i = (size_t) ver + 1; i < MIGRATIONS_COUNT; i++) {
        printf("Apply migration: %zu ...\n", i);

        if ((*migrations[i])() != 0) {
            printf("Can't apply %zu migration\n", i);
            migration_rollback();
            return 1;
        }
        if (set_current_version((int) i) != 0) {
            migration_rollback();
            return 1;
        }
    }

    if (migration_commit() != 0) {
        migration_rollback();
        return 1;
    }
    return 0;
}


/* Version guardada en la tabla `version` (la crea sql/params.sql; una base sin
 * esa tabla esta incompleta y database_init() ya la rechazo). Sin fila o con un
 * numero negativo la version es AMBIGUA: no se puede saber que migraciones
 * corrieron -- y la 2 no es repetible -- asi que se aborta sin tocar nada. */
static int get_current_version(int * ver) {
    sqlite3_stmt *stmt;
    int           rc = sqlite3_prepare_v2(db, "SELECT id FROM version LIMIT 1", -1, &stmt, 0);

    if (rc != SQLITE_OK) {
        printf("Failed prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        *ver = sqlite3_column_int(stmt, 0);
    } else if (rc == SQLITE_DONE) {
        printf("The version table has no row: database state is ambiguous, refusing to migrate\n");
        sqlite3_finalize(stmt);
        return 1;
    } else {
        printf("Cannot read the database version: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);

    if (*ver < 0) {
        printf("Invalid database version %d: refusing to migrate\n", *ver);
        return 1;
    }
    return 0;
}

static int set_current_version(int ver) {
    sqlite3_stmt *stmt;
    int           rc = sqlite3_prepare_v2(db, "UPDATE version SET id=?", -1, &stmt, 0);

    if (rc != SQLITE_OK) {
        printf("Failed prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    sqlite3_bind_int(stmt, 1, ver);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        printf("Cannot set version %d: %s\n", ver, sqlite3_errmsg(db));
        return 1;
    }
    if (sqlite3_changes(db) != 1) {
        printf("Cannot set version %d: the version table changed in %d rows\n", ver, sqlite3_changes(db));
        return 1;
    }
    return 0;
}

/*
 * Fixtures de params.db para los tests de persistencia (issue #213).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "db_fixtures.h"

#include <sqlite3.h>
#include <stdio.h>
#include <unistd.h>

#define SQL_VERSION       "CREATE TABLE version(id INT NOT NULL DEFAULT 0);"
#define SQL_PARAMS        "CREATE TABLE params(name TEXT PRIMARY KEY, val INTEGER);"
#define SQL_BANDS         "CREATE TABLE bands(id INTEGER PRIMARY KEY, name TEXT, start_freq INTEGER, stop_freq INTEGER, type INTEGER);"
#define SQL_BAND_PARAMS   "CREATE TABLE band_params(bands_id INTEGER, name TEXT, val INTEGER, UNIQUE(bands_id, name) ON CONFLICT REPLACE);"
#define SQL_MODE_PARAMS   "CREATE TABLE mode_params(mode INTEGER, name TEXT, val INTEGER, UNIQUE(mode, name) ON CONFLICT REPLACE);"
#define SQL_MEMORY        "CREATE TABLE memory(id INTEGER, name TEXT, val INTEGER, UNIQUE(id, name) ON CONFLICT REPLACE);"
#define SQL_ATU           "CREATE TABLE atu(ant INTEGER, freq INTEGER, val INTEGER, UNIQUE(ant, freq) ON CONFLICT REPLACE);"
#define SQL_TRANSVERTER   "CREATE TABLE transverter(id INTEGER, name TEXT, val INTEGER, UNIQUE(id, name) ON CONFLICT REPLACE);"
#define SQL_MSG_CW        "CREATE TABLE msg_cw(id INTEGER PRIMARY KEY, val INTEGER);"
#define SQL_DIGITAL_MODES "CREATE TABLE digital_modes(id INTEGER PRIMARY KEY AUTOINCREMENT, label varchar(64) NOT NULL, freq INTEGER NOT NULL CHECK(freq > 0), mode INTEGER NOT NULL DEFAULT 3 CHECK(mode >= 0 AND mode <= 7), type INTEGER NOT NULL, CONSTRAINT freq_type_uniq UNIQUE(freq, type));"
#define SQL_DFL_MEMORIES  "CREATE TABLE dfl_filter_memories(family INTEGER NOT NULL CHECK(family BETWEEN 1 AND 4), memory_slot INTEGER NOT NULL CHECK(memory_slot BETWEEN 1 AND 2), low_hz INTEGER NOT NULL, high_hz INTEGER NOT NULL, PRIMARY KEY(family, memory_slot));"
#define SQL_DFL_FAVORITES "CREATE TABLE dfl_filter_favorites(family INTEGER NOT NULL, quick_slot INTEGER NOT NULL, source INTEGER NOT NULL, PRIMARY KEY(family, quick_slot));"

#define SQL_FULL \
    SQL_VERSION SQL_PARAMS SQL_BANDS SQL_BAND_PARAMS SQL_MODE_PARAMS SQL_MEMORY SQL_ATU \
    SQL_TRANSVERTER SQL_MSG_CW SQL_DIGITAL_MODES SQL_DFL_MEMORIES SQL_DFL_FAVORITES

/* Filas de una base sana: un preset por tabla para poder afirmar que no se toca. */
#define ROWS_VALID \
    "INSERT INTO params(name,val) VALUES('band',7),('vol',30),('theme_name','Aurora');" \
    "INSERT INTO bands(id,name,start_freq,stop_freq,type) VALUES(1,'40m',7000000,7200000,0);" \
    "INSERT INTO band_params VALUES(1,'vfoa_freq',7074000),(1,'vfob_freq',7025000);" \
    "INSERT INTO mode_params VALUES(3,'filter_low',300),(3,'filter_high',2700);" \
    "INSERT INTO memory VALUES(1,'label','home'),(1,'vfoa_freq',7074000);" \
    "INSERT INTO atu VALUES(1,7025000,120);" \
    "INSERT INTO dfl_filter_memories VALUES(2,1,300,2700);"

static bool exec_sql(sqlite3 *h, const char *sql) {
    return sqlite3_exec(h, sql, NULL, NULL, NULL) == SQLITE_OK;
}

bool db_fixture_make(const char *path, db_fixture_t kind) {
    sqlite3 *h    = NULL;
    bool     ok   = false;

    unlink(path);
    if (sqlite3_open(path, &h) != SQLITE_OK) {
        sqlite3_close(h);
        return false;
    }

    switch (kind) {
        case DB_FIX_VALID:
            ok = exec_sql(h, SQL_FULL "INSERT INTO version(id) VALUES(5);" ROWS_VALID);
            break;
        case DB_FIX_NO_BANDS:
            ok = exec_sql(h,
                SQL_VERSION SQL_PARAMS SQL_BAND_PARAMS SQL_MODE_PARAMS SQL_MEMORY SQL_ATU
                "INSERT INTO version(id) VALUES(5);"
                "INSERT INTO params(name,val) VALUES('theme_name','Aurora');");
            break;
        case DB_FIX_NO_VERSION_ROW:
            ok = exec_sql(h,
                SQL_FULL
                "INSERT INTO params(name,val) VALUES('band',7),('theme_name','Aurora');"
                "INSERT INTO bands(id,name,start_freq,stop_freq,type) VALUES(1,'40m',7000000,7200000,0);"
                "INSERT INTO atu VALUES(1,7025000,120);");
            break;
        case DB_FIX_BAD_VERSION:
            ok = exec_sql(h, SQL_FULL "INSERT INTO version(id) VALUES(9);" ROWS_VALID);
            break;
        case DB_FIX_V1_ATU_INDEX:
            ok = exec_sql(h,
                SQL_FULL
                "INSERT INTO version(id) VALUES(1);"
                "INSERT INTO params(name,val) VALUES('theme_name','Aurora');"
                "INSERT INTO bands(id,name,start_freq,stop_freq,type) VALUES(201,'CB A',26065000,26505000,1);"
                "INSERT INTO band_params VALUES(201,'vfoa_freq',26055000);"
                "INSERT INTO atu VALUES(1,3,100),(1,5,200);");
            break;
        case DB_FIX_V1_ATU_HZ:
            ok = exec_sql(h,
                SQL_FULL
                "INSERT INTO version(id) VALUES(1);"
                "INSERT INTO params(name,val) VALUES('theme_name','Aurora');"
                "INSERT INTO atu VALUES(1,7025000,100),(1,14250000,200);");
            break;
        case DB_FIX_V1_ATU_MIXED:
            ok = exec_sql(h,
                SQL_FULL
                "INSERT INTO version(id) VALUES(1);"
                "INSERT INTO params(name,val) VALUES('theme_name','Aurora');"
                "INSERT INTO atu VALUES(1,3,100),(1,7025000,200);");
            break;
        case DB_FIX_V0_CONFLICT:
            ok = exec_sql(h,
                SQL_FULL
                "INSERT INTO version(id) VALUES(0);"
                "INSERT INTO params(name,val) VALUES('theme_name','Aurora');"
                "INSERT INTO memory VALUES(100,'label','FT8'),(100,'vfoa_freq',14074000),(100,'vfoa_mode',3);"
                "INSERT INTO digital_modes(label,freq,mode,type) VALUES('pre',14074000,3,0);");
            break;
        case DB_FIX_TEMPLATE_BAD:
            ok = exec_sql(h,
                SQL_VERSION SQL_PARAMS SQL_BANDS SQL_BAND_PARAMS SQL_MODE_PARAMS SQL_ATU
                "INSERT INTO version(id) VALUES(5);");
            break;
    }
    sqlite3_close(h);
    return ok;
}

const char *db_fixture_name(db_fixture_t kind) {
    switch (kind) {
        case DB_FIX_VALID:          return "valid";
        case DB_FIX_NO_BANDS:       return "no-bands";
        case DB_FIX_NO_VERSION_ROW: return "no-version-row";
        case DB_FIX_BAD_VERSION:    return "bad-version";
        case DB_FIX_V1_ATU_INDEX:   return "v1-atu-index";
        case DB_FIX_V1_ATU_HZ:      return "v1-atu-hz";
        case DB_FIX_V1_ATU_MIXED:   return "v1-atu-mixed";
        case DB_FIX_V0_CONFLICT:    return "v0-conflict";
        case DB_FIX_TEMPLATE_BAD:   return "template-bad";
    }
    return "?";
}

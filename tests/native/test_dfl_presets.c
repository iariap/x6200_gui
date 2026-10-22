/*
 * Presets de filtro del DFL (issue #183): catalogo, modelo, persistencia y
 * aplicacion segura.
 *
 * Corre contra una base sqlite3 REAL en memoria con las tablas del contrato
 * (dfl_filter_memories / dfl_filter_favorites) y contra el espejo exacto de los
 * setters de src/cfg/mode.c (tests/native/dfl_presets_stubs.c). El modulo real
 * que se prueba es src/dfl_presets.c + src/dfl_presets_store.c.
 *
 * Cubre los puntos de la seccion 7 del issue que son de modelo: catalogo exacto
 * de 4 familias x 3, no auto-aplicar, coincidencia/`Personalizado`, aplicacion
 * desde un estado incompatible sin half-apply, guardar/recuperar Mem1/2,
 * reasignar favoritos por ID, memoria vacia inaccesible, aislamiento por familia,
 * favorito huerfano con fallback seguro, fallo de SQL sin exito, y que NADIE
 * escriba cfg_cur.filter.bw ni el par real.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <sqlite3.h>
#include <string.h>

#include "db.h"
#include "cfg/mode.h"
#include "dfl_presets.h"
#include "dfl_presets_probe.h"
#include "test_framework.h"

static int exec(const char *sql) {
    return sqlite3_exec(db, sql, NULL, NULL, NULL);
}

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

/* Las dos tablas del contrato (seccion 4). */
static void create_tables(void) {
    CHECK(exec("CREATE TABLE IF NOT EXISTS dfl_filter_memories ("
               " family INTEGER NOT NULL CHECK(family BETWEEN 1 AND 4),"
               " memory_slot INTEGER NOT NULL CHECK(memory_slot BETWEEN 1 AND 2),"
               " low_hz INTEGER NOT NULL CHECK(low_hz BETWEEN -10000 AND 9999),"
               " high_hz INTEGER NOT NULL CHECK(high_hz BETWEEN 1 AND 10000),"
               " PRIMARY KEY(family, memory_slot), CHECK(high_hz > low_hz));"
               "CREATE TABLE IF NOT EXISTS dfl_filter_favorites ("
               " family INTEGER NOT NULL CHECK(family BETWEEN 1 AND 4),"
               " quick_slot INTEGER NOT NULL CHECK(quick_slot BETWEEN 1 AND 3),"
               " source INTEGER NOT NULL CHECK(source BETWEEN 1 AND 5),"
               " PRIMARY KEY(family, quick_slot));") == SQLITE_OK);
}

static void clear_tables(void) {
    CHECK(exec("DELETE FROM dfl_filter_memories; DELETE FROM dfl_filter_favorites;") == SQLITE_OK);
}

/* --- Catalogo exacto (seccion 3) ------------------------------------------- */

static void check_catalog(void) {
    static const struct {
        dfl_family_t family;
        const char  *name[3];
        int32_t      low[3];
        int32_t      high[3];
    } expected[4] = {
        { DFL_FAMILY_SSB,  { "Wide", "Normal", "DX" },     { 100, 150, 300 },     { 3400, 2850, 2400 } },
        { DFL_FAMILY_DIGI, { "Wide", "Normal", "Focus" },  { 100, 50, 1200 },     { 3500, 2950, 1800 } },
        { DFL_FAMILY_CW,   { "Wide", "Normal", "DX" },     { -400, -250, -150 },  { 400, 250, 150 } },
        { DFL_FAMILY_AM,   { "Wide", "Normal", "Narrow" }, { 0, 0, 0 },           { 6000, 4500, 2500 } },
    };

    for (int f = 0; f < 4; f++) {
        for (int i = 0; i < DFL_FACTORY_COUNT; i++) {
            int32_t low  = 0;
            int32_t high = 0;

            CHECK(dfl_factory_values(expected[f].family, i, &low, &high));
            CHECK(low == expected[f].low[i]);
            CHECK(high == expected[f].high[i]);
            CHECK(strcmp(dfl_factory_name(expected[f].family, i), expected[f].name[i]) == 0);
            CHECK(dfl_values_legal(expected[f].family, low, high));
        }
        /* Fuera de rango: no hay cuarto preset. */
        CHECK(!dfl_factory_values(expected[f].family, 3, NULL, NULL));
    }
}

static void check_family_of_mode(void) {
    CHECK(dfl_family_of_mode(x6200_mode_usb) == DFL_FAMILY_SSB);
    CHECK(dfl_family_of_mode(x6200_mode_lsb) == DFL_FAMILY_SSB);
    CHECK(dfl_family_of_mode(x6200_mode_usb_dig) == DFL_FAMILY_DIGI);
    CHECK(dfl_family_of_mode(x6200_mode_lsb_dig) == DFL_FAMILY_DIGI);
    CHECK(dfl_family_of_mode(x6200_mode_cw) == DFL_FAMILY_CW);
    CHECK(dfl_family_of_mode(x6200_mode_cwr) == DFL_FAMILY_CW);
    CHECK(dfl_family_of_mode(x6200_mode_am) == DFL_FAMILY_AM);
    CHECK(dfl_family_of_mode(x6200_mode_sam) == DFL_FAMILY_AM);
    /* NFM/WFM no tienen catalogo aprobado. */
    CHECK(dfl_family_of_mode(x6200_mode_nfm) == DFL_FAMILY_NONE);
    CHECK(dfl_family_of_mode(x6200_mode_wfm) == DFL_FAMILY_NONE);
    CHECK(dfl_family_of_mode(99) == DFL_FAMILY_NONE);
}

static void check_legal(void) {
    CHECK(!dfl_values_legal(DFL_FAMILY_SSB, -100, 2400));   /* LOW negativo no en SSB */
    CHECK(!dfl_values_legal(DFL_FAMILY_DIGI, -1, 2400));
    CHECK(dfl_values_legal(DFL_FAMILY_CW, -400, 400));      /* firmado si en CW */
    CHECK(!dfl_values_legal(DFL_FAMILY_AM, 100, 2400));     /* AM exige LOW=0 */
    CHECK(dfl_values_legal(DFL_FAMILY_AM, 0, 2500));
    CHECK(!dfl_values_legal(DFL_FAMILY_SSB, 300, 300));     /* high > low */
    CHECK(!dfl_values_legal(DFL_FAMILY_SSB, 300, 200));
    CHECK(!dfl_values_legal(DFL_FAMILY_SSB, 0, 10001));     /* tope del firmware */
    CHECK(!dfl_values_legal(DFL_FAMILY_NONE, 0, 2400));     /* sin familia, sin preset */
}

/* --- Modelo y no-auto-aplicar --------------------------------------------- */

static void check_no_auto_apply(void) {
    dfl_pt_reset(x6200_mode_usb, 1234, 5678);
    dfl_presets_init();

    /* Abrir el DFL / encender / inicializar NO aplica nada. */
    CHECK(dfl_pt_low() == 1234);
    CHECK(dfl_pt_high() == 5678);
    CHECK(dfl_pt_bw_writes() == 0);
    CHECK(dfl_pt_real_writes() == 0);
}

static void check_defaults_and_match(void) {
    clear_tables();
    dfl_presets_reload();

    /* Sin filas: favorito n -> preset n. */
    CHECK(dfl_presets_favorite_source(DFL_FAMILY_SSB, 1) == DFL_SRC_FACTORY_1);
    CHECK(dfl_presets_favorite_source(DFL_FAMILY_SSB, 2) == DFL_SRC_FACTORY_2);
    CHECK(dfl_presets_favorite_source(DFL_FAMILY_SSB, 3) == DFL_SRC_FACTORY_3);

    /* Coincidencia exacta por par, primer slot. */
    CHECK(dfl_matching_slot(DFL_FAMILY_SSB, 100, 3400) == 1);
    CHECK(dfl_matching_slot(DFL_FAMILY_SSB, 150, 2850) == 2);
    CHECK(dfl_matching_slot(DFL_FAMILY_SSB, 300, 2400) == 3);
    CHECK(dfl_matching_slot(DFL_FAMILY_SSB, 200, 2000) == -1);   /* Personalizado */

    /* Los valores del favorito salen de su fuente. */
    int32_t low  = 0;
    int32_t high = 0;
    CHECK(dfl_presets_favorite_values(DFL_FAMILY_SSB, 1, &low, &high));
    CHECK(low == 100 && high == 3400);
    CHECK(strstr(dfl_presets_favorite_label(DFL_FAMILY_SSB, 1), "Wide") != NULL);
    CHECK(strstr(dfl_presets_favorite_label(DFL_FAMILY_SSB, 1), "3300 Hz") != NULL);
}

/* --- Aplicacion segura ----------------------------------------------------- */

static void check_apply_favorite(void) {
    dfl_pt_reset(x6200_mode_usb, 150, 2850);
    dfl_presets_reload();

    CHECK(dfl_apply_favorite(3) == DFL_APPLY_OK);   /* DX 300/2400 */
    CHECK(dfl_pt_low() == 300);
    CHECK(dfl_pt_high() == 2400);
    CHECK(dfl_pt_bw() == 2100);                     /* BW derivado, no escrito */
    CHECK(dfl_pt_bw_writes() == 0);
    CHECK(dfl_pt_real_writes() == 0);
}

/* Estado previo incompatible con el orden ingenuo: LOW=3000 / HIGH=4000 -> DX
 * 300/2400. La guarda de cfg_mode_set_high_filter() exige high > low, asi que el
 * LOW tiene que ir primero; si no, quedaria a medias. */
static void check_apply_incompatible_state(void) {
    dfl_pt_reset(x6200_mode_usb, 3000, 4000);
    dfl_presets_reload();

    CHECK(dfl_apply_source(DFL_FAMILY_SSB, DFL_SRC_FACTORY_3) == DFL_APPLY_OK);
    CHECK(dfl_pt_low() == 300);
    CHECK(dfl_pt_high() == 2400);
    CHECK(dfl_pt_bw() == 2100);
    CHECK(dfl_pt_bw_writes() == 0);
    CHECK(dfl_pt_real_writes() == 0);

    /* El otro sentido tambien: LOW=100/HIGH=3400 -> Focus DIGI no aplica (otra
     * familia), pero CW Wide desde un par angosto si. */
    dfl_pt_reset(x6200_mode_cwr, -150, 150);
    CHECK(dfl_apply_source(DFL_FAMILY_CW, DFL_SRC_FACTORY_1) == DFL_APPLY_OK);   /* -400/400 */
    CHECK(dfl_pt_low() == -400);
    CHECK(dfl_pt_high() == 400);
    CHECK(dfl_pt_bw() == 800);
}

static void check_apply_am(void) {
    dfl_pt_reset(x6200_mode_sam, 0, 4500);
    CHECK(dfl_apply_source(DFL_FAMILY_AM, DFL_SRC_FACTORY_1) == DFL_APPLY_OK);   /* 0/6000 */
    CHECK(dfl_pt_low() == 0);
    CHECK(dfl_pt_high() == 6000);
    CHECK(dfl_pt_bw() == 6000);   /* el BW guardado es 6000, no 12000 */
    CHECK(dfl_pt_real_writes() == 0);
}

static void check_apply_invalid(void) {
    dfl_pt_reset(x6200_mode_usb, 100, 3400);
    dfl_presets_reload();

    CHECK(dfl_apply_source(DFL_FAMILY_SSB, DFL_SRC_NONE) == DFL_APPLY_INVALID_SOURCE);
    CHECK(dfl_apply_source(DFL_FAMILY_SSB, DFL_SRC_MEM_1) == DFL_APPLY_INVALID_SOURCE);   /* vacia */
    CHECK(dfl_apply_source(DFL_FAMILY_NONE, DFL_SRC_FACTORY_1) == DFL_APPLY_INVALID_SOURCE);
    CHECK(dfl_apply_memory(1) == DFL_APPLY_INVALID_SOURCE);
    /* Nada cambio. */
    CHECK(dfl_pt_low() == 100 && dfl_pt_high() == 3400);
}

/* --- Guardar / recuperar / reasignar -------------------------------------- */

static void check_save_restore_assign(void) {
    clear_tables();
    dfl_presets_reload();
    dfl_pt_reset(x6200_mode_usb, 500, 2500);

    CHECK(dfl_save_memory(DFL_FAMILY_SSB, 1, 500, 2500));
    CHECK(dfl_presets_memory_present(DFL_FAMILY_SSB, 1));
    CHECK(query_int("SELECT low_hz FROM dfl_filter_memories WHERE family=1 AND memory_slot=1") == 500);
    CHECK(query_int("SELECT high_hz FROM dfl_filter_memories WHERE family=1 AND memory_slot=1") == 2500);

    /* Sobrevive a una recarga (proceso/reinicio simulados). */
    dfl_presets_reload();
    CHECK(dfl_presets_memory_present(DFL_FAMILY_SSB, 1));

    int32_t low  = 0;
    int32_t high = 0;
    CHECK(dfl_presets_memory_values(DFL_FAMILY_SSB, 1, &low, &high));
    CHECK(low == 500 && high == 2500);

    /* Aplicar Mem 1. */
    dfl_pt_reset(x6200_mode_usb, 100, 3400);
    CHECK(dfl_apply_memory(1) == DFL_APPLY_OK);
    CHECK(dfl_pt_low() == 500 && dfl_pt_high() == 2500);

    /* Reasignar el favorito 2 a Mem 1: se guarda el ID, no copias. */
    CHECK(dfl_assign_favorite(DFL_FAMILY_SSB, 2, DFL_SRC_MEM_1));
    CHECK(query_int("SELECT source FROM dfl_filter_favorites WHERE family=1 AND quick_slot=2") == DFL_SRC_MEM_1);
    CHECK(dfl_presets_favorite_source(DFL_FAMILY_SSB, 2) == DFL_SRC_MEM_1);

    CHECK(dfl_presets_favorite_values(DFL_FAMILY_SSB, 2, &low, &high));
    CHECK(low == 500 && high == 2500);
    CHECK(strstr(dfl_presets_favorite_label(DFL_FAMILY_SSB, 2), "Mem 1") != NULL);

    /* Sobrescribir Mem 1: el favorito refleja el contenido NUEVO. */
    CHECK(dfl_save_memory(DFL_FAMILY_SSB, 1, 700, 2700));
    CHECK(dfl_presets_favorite_values(DFL_FAMILY_SSB, 2, &low, &high));
    CHECK(low == 700 && high == 2700);

    /* Mem 2 vacia: no se puede asignar. */
    CHECK(!dfl_assign_favorite(DFL_FAMILY_SSB, 3, DFL_SRC_MEM_2));
    CHECK(dfl_presets_favorite_source(DFL_FAMILY_SSB, 3) == DFL_SRC_FACTORY_3);

    /* Tres favoritos a la misma memoria: solo el primero coincide. */
    CHECK(dfl_assign_favorite(DFL_FAMILY_SSB, 1, DFL_SRC_MEM_1));
    CHECK(dfl_matching_slot(DFL_FAMILY_SSB, 700, 2700) == 1);
    CHECK(dfl_presets_favorite_source(DFL_FAMILY_SSB, 1) == DFL_SRC_MEM_1);
    CHECK(dfl_presets_favorite_source(DFL_FAMILY_SSB, 2) == DFL_SRC_MEM_1);
    CHECK(dfl_presets_favorite_source(DFL_FAMILY_SSB, 3) == DFL_SRC_FACTORY_3);
}

static void check_family_isolation(void) {
    clear_tables();
    dfl_presets_reload();

    CHECK(dfl_save_memory(DFL_FAMILY_SSB, 1, 500, 2500));
    CHECK(dfl_save_memory(DFL_FAMILY_DIGI, 1, 1200, 1800));

    /* La memoria de SSB no se ve desde DIGI ni al reves. */
    int32_t low  = 0;
    int32_t high = 0;
    CHECK(dfl_presets_memory_values(DFL_FAMILY_SSB, 1, &low, &high) && low == 500 && high == 2500);
    CHECK(dfl_presets_memory_values(DFL_FAMILY_DIGI, 1, &low, &high) && low == 1200 && high == 1800);
    CHECK(!dfl_presets_memory_present(DFL_FAMILY_CW, 1));
    CHECK(!dfl_presets_memory_present(DFL_FAMILY_AM, 1));

    /* Y el catalogo de cada familia es el suyo. */
    CHECK(dfl_factory_values(DFL_FAMILY_DIGI, 2, &low, &high) && low == 1200 && high == 1800);
    CHECK(dfl_factory_values(DFL_FAMILY_AM, 2, &low, &high) && low == 0 && high == 2500);
}

static void check_orphan_favorite(void) {
    clear_tables();
    dfl_presets_reload();

    /* Fila huerfana: el favorito apunta a Mem 2, que no existe. */
    CHECK(exec("INSERT INTO dfl_filter_favorites(family, quick_slot, source) VALUES(1, 2, 5)") == SQLITE_OK);
    dfl_presets_reload();

    CHECK(dfl_presets_favorite_source(DFL_FAMILY_SSB, 2) == DFL_SRC_FACTORY_2);   /* fallback seguro */
    int32_t low  = 0;
    int32_t high = 0;
    CHECK(dfl_presets_favorite_values(DFL_FAMILY_SSB, 2, &low, &high));
    CHECK(low == 150 && high == 2850);
}

/* --- Fallos de persistencia ----------------------------------------------- */

static void check_sql_failure(void) {
    /* Sin base: guardar falla y NO se informa exito. */
    sqlite3 *saved = db;
    db = NULL;

    dfl_pt_reset(x6200_mode_usb, 500, 2500);
    CHECK(!dfl_save_memory(DFL_FAMILY_SSB, 1, 500, 2500));
    CHECK(dfl_presets_last_error()[0] != '\0');
    CHECK(!dfl_presets_memory_present(DFL_FAMILY_SSB, 1));   /* la cache no miente */

    db = saved;

    /* Tabla ausente: guardar falla, no se toca nada mas. */
    CHECK(exec("DROP TABLE dfl_filter_memories") == SQLITE_OK);
    CHECK(!dfl_save_memory(DFL_FAMILY_SSB, 1, 500, 2500));
    CHECK(dfl_presets_last_error()[0] != '\0');
    create_tables();
}

void test_dfl_presets(void) {
    CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
    create_tables();

    check_catalog();
    check_family_of_mode();
    check_legal();
    check_no_auto_apply();
    check_defaults_and_match();
    check_apply_favorite();
    check_apply_incompatible_state();
    check_apply_am();
    check_apply_invalid();
    check_save_restore_assign();
    check_family_isolation();
    check_orphan_favorite();
    check_sql_failure();

    sqlite3_close(db);
    db = NULL;
}

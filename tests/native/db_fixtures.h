/*
 * Fixtures de params.db para los tests de persistencia (issue #213).
 *
 * Compartidos por el test del host (tests/native/test_db_persistence.c) y por el
 * end-to-end con las rutas reales (tests/native/db_end_to_end.c), para que la base
 * que se mide sea la misma en los dos lados.
 *
 * El esquema completo es el de sql/params.sql (lo que instala la plantilla de la
 * ROM); cada fixture agrega solo las filas que su caso necesita.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>

typedef enum {
    DB_FIX_VALID,          /* version 5, esquema completo, presets */
    DB_FIX_NO_BANDS,       /* version 5, sin la tabla bands */
    DB_FIX_NO_VERSION_ROW, /* esquema completo, tabla version sin fila */
    DB_FIX_BAD_VERSION,    /* version 9: mas nueva que el firmware */
    DB_FIX_V1_ATU_INDEX,   /* version 1, atu en la forma vieja (indices) */
    DB_FIX_V1_ATU_HZ,      /* version 1, atu YA en Hz (migracion 2 ya hecha) */
    DB_FIX_V1_ATU_MIXED,   /* version 1, atu con las dos formas: ambigua */
    DB_FIX_V0_CONFLICT,    /* version 0: la migracion _1 choca con digital_modes */
    DB_FIX_TEMPLATE_BAD,   /* plantilla sin la tabla memory */
} db_fixture_t;

/* Crea (o reemplaza) la base en `path`. false si no se pudo. */
bool db_fixture_make(const char *path, db_fixture_t kind);

/* Nombre del fixture (para logs y evidencia). */
const char *db_fixture_name(db_fixture_t kind);

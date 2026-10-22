/*
 * Doble EN MEMORIA del almacen de presets del DFL (issue #183) para el arnes de
 * host: el arnes no tiene `params.db` (no compila src/params/db.c ni linkea
 * sqlite3), asi que el modelo real (src/dfl_presets.c) se compila contra este
 * doble con la MISMA interfaz (src/dfl_presets_store.h).
 *
 * El SQL de verdad (tablas, transaccion, errores BUSY/IO/tabla ausente) lo cubre
 * el test nativo tests/native/test_dfl_presets.c contra sqlite3 real: aca solo se
 * prueba la UI (paginas, etiquetas, disabled, navegacion y feedback).
 *
 * `mock_dfl_store_fail(true)` fuerza el fallo de escritura para el fixture del
 * feedback veraz ("nunca Guardado si no se guardo").
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "dfl_presets_store.h"

#include "mock_harness.h"

#include <string.h>

typedef struct {
    int     family;
    int     slot;
    int32_t low;
    int32_t high;
} mem_row_t;

typedef struct {
    int family;
    int slot;
    int source;
} fav_row_t;

static mem_row_t memories[8];
static int       memories_n;
static fav_row_t favorites[12];
static int       favorites_n;

static bool      fail_writes;
static char      last_error[96];

void mock_dfl_store_reset(void) {
    memories_n   = 0;
    favorites_n  = 0;
    fail_writes  = false;
    last_error[0] = '\0';
}

void mock_dfl_store_fail(bool on) {
    fail_writes = on;
}

const char *dfl_store_last_error(void) {
    return last_error[0] ? last_error : "mock: sin detalle";
}

bool dfl_store_available(void) {
    return true;
}

bool dfl_store_load_memory(int family, int slot, int32_t *low, int32_t *high) {
    for (int i = 0; i < memories_n; i++) {
        if (memories[i].family == family && memories[i].slot == slot) {
            *low  = memories[i].low;
            *high = memories[i].high;
            return true;
        }
    }
    return false;
}

bool dfl_store_load_favorite(int family, int slot, int32_t *source) {
    for (int i = 0; i < favorites_n; i++) {
        if (favorites[i].family == family && favorites[i].slot == slot) {
            *source = favorites[i].source;
            return true;
        }
    }
    return false;
}

bool dfl_store_save_memory(int family, int slot, int32_t low, int32_t high) {
    if (fail_writes) {
        strcpy(last_error, "mock: escritura forzada a fallar");
        return false;
    }

    for (int i = 0; i < memories_n; i++) {
        if (memories[i].family == family && memories[i].slot == slot) {
            memories[i].low  = low;
            memories[i].high = high;
            return true;
        }
    }

    if (memories_n >= (int) (sizeof(memories) / sizeof(memories[0]))) {
        return false;
    }
    memories[memories_n].family = family;
    memories[memories_n].slot   = slot;
    memories[memories_n].low    = low;
    memories[memories_n].high   = high;
    memories_n++;
    return true;
}

bool dfl_store_save_favorite(int family, int slot, int32_t source) {
    if (fail_writes) {
        strcpy(last_error, "mock: escritura forzada a fallar");
        return false;
    }

    for (int i = 0; i < favorites_n; i++) {
        if (favorites[i].family == family && favorites[i].slot == slot) {
            favorites[i].source = source;
            return true;
        }
    }

    if (favorites_n >= (int) (sizeof(favorites) / sizeof(favorites[0]))) {
        return false;
    }
    favorites[favorites_n].family = family;
    favorites[favorites_n].slot   = slot;
    favorites[favorites_n].source = source;
    favorites_n++;
    return true;
}

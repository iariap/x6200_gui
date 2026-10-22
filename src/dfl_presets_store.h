/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Persistencia de los presets del DFL (issue #183) en `params.db`.
 *
 *  Adaptador SQLite separado del modelo (src/dfl_presets.c): el modelo no sabe
 *  de SQL y el arnes de host compila el modelo contra un doble en memoria (no
 *  hay params.db en el host). Las tablas las crea la migracion versionada
 *  (src/params/migrations.c, _5) y sql/params.sql para instalaciones nuevas.
 *
 *  Todas las funciones devuelven false ante cualquier error de SQLite (BUSY,
 *  I/O, tabla ausente) y dejan el motivo en dfl_store_last_error(): la UI nunca
 *  puede informar "Guardado" si la fila no quedo escrita.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* true si hay base abierta. Con false, todas las lecturas dicen "sin fila" y
 * las escrituras fallan (nunca corrompen nada). */
bool dfl_store_available(void);

/* Lecturas: true solo si habia fila. `low`/`high`/`source` quedan sin tocar si
 * no hay fila. */
bool dfl_store_load_memory(int family, int slot, int32_t *low, int32_t *high);
bool dfl_store_load_favorite(int family, int slot, int32_t *source);

/* Escrituras: UNA transaccion por guardado (INSERT OR REPLACE). */
bool dfl_store_save_memory(int family, int slot, int32_t low, int32_t high);
bool dfl_store_save_favorite(int family, int slot, int32_t source);

const char *dfl_store_last_error(void);

#ifdef __cplusplus
}
#endif

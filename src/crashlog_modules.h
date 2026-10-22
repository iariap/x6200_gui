/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Tabla de modulos cargados para el CRASHLOG (issue #131).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRASHLOG_MD5_HEX_LEN    32
#define CRASHLOG_MOD_PATH_MAX   96
#define CRASHLOG_MOD_MAX        96
#define CRASHLOG_MOD_BLOCK_MAX  (CRASHLOG_MOD_MAX * 160)

typedef struct {
    uintptr_t base;
    char      md5[CRASHLOG_MD5_HEX_LEN + 1];
    char      path[CRASHLOG_MOD_PATH_MAX];
} crashlog_mod_t;

typedef struct {
    crashlog_mod_t mods[CRASHLOG_MOD_MAX];
    size_t         count;
    size_t         omitted;
} crashlog_mod_table_t;

/* Arranque: se llama UNA vez, antes de instalar el handler. Lee
 * /proc/self/maps, calcula el md5 de cada archivo y deja el bloque de lineas
 * CRASHLOG-MOD ya formateado. Sin heap. Si /proc no esta montado (chroot del
 * banco), cae a dl_iterate_phdr con la misma informacion. */
void crashlog_modules_init(const char *exe_path);

/* Bloque preformateado que el handler escribe tal cual (puede ser NULL/0 si el
 * arranque no pudo leer /proc/self/maps). El handler NO formatea ni aloca. */
const char *crashlog_modules_block(size_t *len);
size_t      crashlog_modules_count(void);
const crashlog_mod_t *crashlog_modules_first(void);

/* Para el test y para reusar en otro contexto. */
void   crashlog_mod_table_reset(crashlog_mod_table_t *table);
bool   crashlog_mod_table_add(crashlog_mod_table_t *table, uintptr_t base,
                              const char *md5, const char *path);
size_t crashlog_mod_table_add_from_maps(crashlog_mod_table_t *table, const char *maps_path);
size_t crashlog_mod_render(const crashlog_mod_table_t *table, char *out, size_t out_size);

/* MD5 en hexadecimal minuscula. crashlog_md5_file() devuelve 0 si pudo leer el
 * archivo; si no, deja 32 ceros (visible en el log, nunca una linea rota). */
void crashlog_md5_hex(const void *data, size_t len, char out[CRASHLOG_MD5_HEX_LEN + 1]);
int  crashlog_md5_file(const char *path, char out[CRASHLOG_MD5_HEX_LEN + 1]);

#ifdef __cplusplus
}
#endif

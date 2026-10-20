/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Presets de filtro del DFL (issue #183): catalogo de fabrica por familia,
 *  memorias personales (Mem 1/2) y enlaces de los tres favoritos rapidos.
 *
 *  Este modulo NO usa LVGL: es el modelo (catalogo + cache + validacion) y la
 *  aplicacion SEGURA por los setters vigentes del firmware
 *  (cfg_mode_set_low_filter()/cfg_mode_set_high_filter(), ver src/cfg/mode.c).
 *  La UI (src/buttons.cpp) y el editor inline (src/dfl_inline.c) lo consumen;
 *  la persistencia vive aparte (src/dfl_presets_store.c, SQLite) para que el
 *  arnes de host pueda compilar el modelo sin la base.
 *
 *  Reglas del contrato (#183):
 *    - BW = HIGH - LOW y NO se escribe a mano: lo derivan los observers de
 *      src/cfg/mode.c a partir de los dos limites (on_cur_filter_*_change).
 *    - Nada se aplica solo: aplicar es SIEMPRE una accion explicita del operador.
 *    - Aplicacion sin half-apply: si el segundo limite no entra, se restaura el
 *      par anterior y se avisa.
 *    - El favorito guarda el ID de la fuente (1..5), no una copia de los valores.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Familias del catalogo: 1=SSB (USB/LSB), 2=DIGI (USB-D/LSB-D), 3=CW (CW/CWR),
 * 4=AM (AM/SAM). NFM/WFM y los modos desconocidos no tienen catalogo. */
typedef enum {
    DFL_FAMILY_NONE = 0,
    DFL_FAMILY_SSB  = 1,
    DFL_FAMILY_DIGI = 2,
    DFL_FAMILY_CW   = 3,
    DFL_FAMILY_AM   = 4,
} dfl_family_t;

#define DFL_FAMILY_COUNT 4
#define DFL_QUICK_SLOTS  3
#define DFL_MEM_SLOTS    2
#define DFL_FACTORY_COUNT 3

/* Identificadores estables de la fuente de un favorito (contrato, seccion 4). */
typedef enum {
    DFL_SRC_NONE      = 0,
    DFL_SRC_FACTORY_1 = 1,
    DFL_SRC_FACTORY_2 = 2,
    DFL_SRC_FACTORY_3 = 3,
    DFL_SRC_MEM_1     = 4,
    DFL_SRC_MEM_2     = 5,
} dfl_source_t;

/* Resultado de aplicar una fuente. Nunca se informa OK sin haber releido el
 * Subject y confirmado el par exacto. */
typedef enum {
    DFL_APPLY_OK = 0,
    DFL_APPLY_INVALID_SOURCE,   /* fuente inexistente o memoria vacia */
    DFL_APPLY_INVALID_VALUES,   /* el par no respeta los limites de la familia */
    DFL_APPLY_FAILED,           /* el setter no acepto; se restauro el par previo */
} dfl_apply_result_t;

/* --- Familia --------------------------------------------------------------- */
dfl_family_t dfl_family_of_mode(int mode);          /* NONE en NFM/WFM/undefined */
const char  *dfl_family_name(dfl_family_t family);  /* "SSB" / "DIGI" / ... */
dfl_family_t dfl_current_family(void);              /* la del modo activo (cfg_cur) */

/* --- Catalogo de fabrica --------------------------------------------------- */
/* idx 0..2. Devuelve false si la familia o el indice no existen. */
bool         dfl_factory_values(dfl_family_t family, int idx, int32_t *low, int32_t *high);
const char  *dfl_factory_name(dfl_family_t family, int idx);   /* "Wide"/"Normal"/... */

/* --- Modelo (cache) -------------------------------------------------------- */
/* Carga memorias y enlaces desde la base y arma los defaults (sin fila:
 * source = quick_slot). Idempotente; la llama el arranque. */
void dfl_presets_init(void);

/* Fuerza una recarga (tests). */
void dfl_presets_reload(void);

/* Fuente (1..5) enlazada al favorito `slot` (1..3) de la familia. */
int  dfl_presets_favorite_source(dfl_family_t family, int slot);

/* Valores que resuelve hoy el favorito `slot`: la fuente enlazada. */
bool dfl_presets_favorite_values(dfl_family_t family, int slot, int32_t *low, int32_t *high);

/* Etiqueta de dos lineas del favorito ("Wide\n3300 Hz", "Mem 1\n2900 Hz").
 * Buffer estatico POR SLOT (los tres botones viven a la vez). */
const char *dfl_presets_favorite_label(dfl_family_t family, int slot);

/* Memoria personal (slot 1..2). */
bool        dfl_presets_memory_present(dfl_family_t family, int slot);
bool        dfl_presets_memory_values(dfl_family_t family, int slot, int32_t *low, int32_t *high);
const char *dfl_presets_memory_label(dfl_family_t family, int slot);   /* "Mem 1" / "Mem 1\nvacia" */

/* Nombre de una fuente cualquiera (para la pagina de asignacion y el feedback). */
const char *dfl_source_name(dfl_family_t family, int source);

/* Valores de una fuente (fabricas 1..3 o memorias 4..5). false si no existe o
 * la memoria esta vacia. */
bool dfl_source_values(dfl_family_t family, int source, int32_t *low, int32_t *high);

/* Primer slot cuyo par coincide EXACTAMENTE con (low, high); -1 si ninguno. */
int  dfl_matching_slot(dfl_family_t family, int32_t low, int32_t high);

/* --- Aplicacion ------------------------------------------------------------ */
/* Aplican el par por los setters vigentes, validan y RELEEN. Si el segundo
 * limite no entra, restauran el par anterior y devuelven DFL_APPLY_FAILED. */
dfl_apply_result_t dfl_apply_source(dfl_family_t family, int source);
dfl_apply_result_t dfl_apply_favorite(int slot);   /* 1..3 */
dfl_apply_result_t dfl_apply_memory(int slot);     /* 1..2 */

/* --- Escritura ------------------------------------------------------------- */
/* Guarda el par (validado contra la familia activa) en la memoria `slot`:
 * UNA transaccion. Devuelve false si no se pudo persistir (nunca miente). */
bool dfl_save_memory(dfl_family_t family, int slot, int32_t low, int32_t high);

/* Enlaza el favorito `slot` a la fuente `source` (persiste el ID, no copias).
 * Rechaza fuentes de memoria vacia. */
bool dfl_assign_favorite(dfl_family_t family, int slot, int source);

/* --- Puros (testables sin Subjects) ---------------------------------------- */
bool dfl_values_legal(dfl_family_t family, int32_t low, int32_t high);

/* Orden seguro de los dos setters: true cuando LOW va primero. Es el mismo
 * criterio de dfl_model.h::dfl_reset_low_first(): HIGH primero solo si ya es
 * mayor que el LOW actual (la guarda del setter de LOW exige val < high). */
bool dfl_apply_order_low_first(int32_t tgt_low, int32_t tgt_high, int32_t cur_low, int32_t cur_high);

/* Ultimo error de persistencia, en texto (para el feedback veraz de la UI). */
const char *dfl_presets_last_error(void);

#ifdef __cplusplus
}
#endif

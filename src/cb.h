/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  CB 27 MHz (Banda Ciudadana): planes, mapeo frecuencia <-> canal y ventana de
 *  TX (issue #146).
 *
 *  Logica pura: tablas y aritmetica de Hz, sin LVGL, sin subjects y sin SQLite.
 *  Se compila tal cual en tests/native/ (mismo patron que adaptive_tuning.c), y
 *  la usan el dial (src/main_screen.c), la celda VFO de la barra (src/info.c) y
 *  el interlock de TX (src/radio.c).
 *
 *  La frecuencia se deriva de la TABLA, no de cfg.band_id: el plan de 40 canales
 *  es el mismo en los cuatro grids, y asi el modulo tambien funciona donde no hay
 *  base de datos (el arnes de UI).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Grids del plan (seccion 4.2 del issue). CB_PLAN_NONE solo existe para poder
 * inicializar una variable: no es un grid. */
typedef enum {
    CB_PLAN_NONE = 0,
    CB_PLAN_A    = 1,   /* CB A, id 201: 26,065-26,505 */
    CB_PLAN_B    = 2,   /* CB B, id 202: 26,515-26,955 */
    CB_PLAN_C    = 3,   /* CB C, id 203: 26,965-27,405 (CEPT/FCC/Argentina) */
    CB_PLAN_D    = 4,   /* CB D, id 204: 27,415-27,855 */
} cb_plan_t;

/* Canales del plan CEPT: 1..40 (canal 1 de cada grid = base del grid) */
#define CB_CHANNEL_MIN 1
#define CB_CHANNEL_MAX 40
#define CB_CHANNEL_NONE 0

/* Politica de TX (params.cb_tx_scope, seccion 7.2) */
typedef enum {
    CB_TX_SCOPE_SBC_AR    = 0,  /* solo el SBC argentino, 26,960-27,410 MHz */
    CB_TX_SCOPE_ALL_GRIDS = 1,  /* ademas los rangos de los grids A, B y D */
    CB_TX_SCOPE_OFF       = 2,  /* sin interlock */
} cb_tx_scope_t;

/* Grid al que pertenece `freq` (rango inclusivo canal 1 .. canal 40), o false si
 * no es CB. `plan` puede ser NULL: sirve para preguntar "es CB?" y nada mas. */
bool cb_plan_of_freq(int32_t freq, cb_plan_t *plan);

/* Numero de canal 1..40 si `freq` es EXACTAMENTE un canal del grid; 0 si no (los
 * cinco huecos del plan -26.995 / 27.045 / 27.095 / 27.145 / 27.195- y cualquier
 * frecuencia intermedia dan 0, aunque esten dentro del rango del grid). */
int cb_channel_of_freq(cb_plan_t plan, int32_t freq);

/* Frecuencia del canal 1..40 de `plan`. Un canal fuera de rango se recorta a
 * 1..40; un plan invalido devuelve 0. */
int32_t cb_freq_of_channel(cb_plan_t plan, int channel);

/* Canal al que lleva un giro de `delta` detents desde `freq` (en numero de canal,
 * 1..40, recortado y sin wrap). Con un canal exacto mueve `delta` numeros de
 * canal -por eso 23 -> 24 BAJA de frecuencia (27,255 -> 27,235): el dial en CB
 * cuenta canales, no Hz-. Fuera de canal engancha el primer canal en la direccion
 * del giro (no el mas cercano) y desde ahi suma los detents que falten. Con
 * `delta` 0 devuelve el canal exacto o CB_CHANNEL_NONE; un plan invalido devuelve
 * CB_CHANNEL_NONE. */
int cb_step_channel(cb_plan_t plan, int32_t freq, int delta);

/* true si el PTT de software puede transmitir en `freq` con la politica
 * `tx_scope` (cb_tx_scope_t). Fuera de 26-28 MHz no bloquea nada: las bandas de
 * aficionado siguen igual (el interlock es de la banda CB). */
bool cb_tx_allowed(int32_t freq, int tx_scope);

/* Etiqueta de la celda VFO de la barra: "CH nn" si `freq` es un canal exacto del
 * plan, "CH --" si es CB pero no un canal. Devuelve false (y no escribe `buf`) si
 * la frecuencia no es CB: ahi la celda sigue con su rotulo de siempre (VFO-A /
 * SPL-B). `buf_len` tiene que alcanzar para "CH --". */
bool cb_channel_label(int32_t freq, char *buf, size_t buf_len);

/* Nombre del grid para la fila de frecuencia (issue #169): "CB A".."CB D", y ""
 * para CB_PLAN_NONE (o cualquier valor que no sea un grid). Devuelve un puntero
 * a un literal estatico: no se libera ni se copia. */
const char *cb_plan_name(cb_plan_t plan);

/* Enganches de la GUI/radio (issue #146): entrada/salida de CB (split, modo,
 * potencia, XIT) y tope de potencia. Se implementan en src/radio.c porque
 * necesitan los subjects y el control de la radio; se llaman desde main()
 * DESPUES de params_init(), para que el observer de cfg.band_id corra despues
 * del de src/cfg/band.c. Este header no arrastra LVGL: cb.c sigue siendo puro. */
void cb_hooks_init(void);

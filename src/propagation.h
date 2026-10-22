/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - App "Propagation" (issue #145)
 *
 *  Registro de radiofaros y matematica de la app Propagacion: que beacon del
 *  IBP (NCDXF/IARU) transmite en cada momento, cuanto senal hay sobre el piso de
 *  ruido y que dice eso de la banda. Todo puro: sin radio, sin red, sin LVGL, para
 *  poder correrlo en los tests nativos. El dialogo (src/dialog_propagation.c) es el
 *  unico que toca la radio, y solo para escuchar (RX).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Registro IBP (NCDXF/IARU) ------------------------------------------- */

#define PROPAGATION_IBP_BANDS       5       /* 20/17/15/12/10 m */
#define PROPAGATION_IBP_BEACONS     18      /* 4U1UN .. YV5B */
#define PROPAGATION_IBP_SLOT_S      10      /* cada beacon transmite 10 s */
#define PROPAGATION_IBP_ROTATION_S  180     /* los 18 beacons: una vuelta cada 180 s */

/* Potencia de los dashes dentro del slot: 100 -> 10 -> 1 -> 0.1 W */
#define PROPAGATION_LADDER_STEPS    4

int         propagation_ibp_band_count(void);
int         propagation_ibp_beacon_count(void);

/* Banda IBP `band` (0..4): nombre ("20m") y frecuencia del beacon (Hz). NULL/0
 * fuera de rango. */
const char *propagation_ibp_band_name(int band);
uint32_t    propagation_ibp_freq(int band);

/* Indicativo del beacon `beacon` (0..17) del ciclo, o NULL fuera de rango. */
const char *propagation_ibp_call(int beacon);

/* Banda IBP que contiene `hz`, o -1 cuando la banda no tiene beacons IBP
 * (la IARU no respalda beacons debajo de 14 MHz: 80/60/40/30 m no tienen). */
int         propagation_ibp_band_of_freq(uint32_t hz);

/* --- Turno de los beacons ------------------------------------------------ */

/* En el slot `s = epoch / 10` de la banda `band` transmite el beacon
 * `(s - band) mod 18` (las bandas van corridas un slot entre si, asi que en todo
 * momento hay un beacon por banda). Devuelve el beacon, el epoch en que empezo su
 * slot y los segundos que le quedan. False si la banda esta fuera de rango o el
 * epoch es negativo. */
bool propagation_slot(int64_t epoch, int band, int *beacon, int64_t *slot_start, int *slot_left);

/* Espera (0..179 s) hasta el proximo slot de `beacon` en `band`, y el epoch de ese
 * slot (0 si su slot es el que corre ahora). False si la banda o el beacon estan
 * fuera de rango. */
bool propagation_beacon_slot(int64_t epoch, int band, int beacon, int64_t *slot_start, int *wait_s);

/* Los beacons que la app mide en un barrido: los PROPAGATION_PLAN_MAX que
 * transmiten DESPUES del slot que corre en `epoch` (un slot ya empezado no se
 * mide: no se habria visto desde su principio). Devuelve cuantos, 0 si la banda
 * no tiene beacons IBP. */
#define PROPAGATION_PLAN_MAX 5
int         propagation_plan(int64_t epoch, int band, int beacons[PROPAGATION_PLAN_MAX]);

/* --- Posicion de cada beacon (para la direccion) ------------------------- */

/* Sitio ("New York") y grilla Maidenhead de 4 caracteres del beacon `beacon` del
 * ciclo, de la tabla de sitios del IBP (NCDXF/IARU: docs/NEW_FEATURES.md,
 * seccion de Propagacion). NULL fuera de rango. */
const char *propagation_ibp_site(int beacon);
const char *propagation_ibp_grid(int beacon);

/* Rumbo inicial (grados 0..359, 0 = norte) de (lat1,lon1) a (lat2,lon2). Sin
 * distancia (los dos puntos son el mismo) devuelve 0. */
int         propagation_bearing(double lat1, double lon1, double lat2, double lon2);

/* Punto cardinal de 8 rumbos de un rumbo: "N", "NE", "E", "SE", "S", "SO", "O",
 * "NO". El valor se normaliza a 0..359 (fuera de rango no sale del arreglo). */
const char *propagation_compass(int bearing_deg);

/* --- Medicion e interpretacion ------------------------------------------- */

/* SNR de la lectura del S-meter contra el piso de ruido, en dB. */
int         propagation_snr(int signal_dbm, int noise_dbm);

typedef enum {
    PROPAGATION_VERDICT_NO_DATA = 0,    /* no hubo medicion: no se inventa veredicto */
    PROPAGATION_VERDICT_CLOSED,
    PROPAGATION_VERDICT_MARGINAL,
    PROPAGATION_VERDICT_OPEN,
} propagation_verdict_t;

/* Veredicto de UNA banda a partir del mejor SNR medido. `heard` = al menos un
 * beacon se distinguio del piso de ruido; sin eso no hay senal que interpretar y
 * el veredicto es CLOSED (la pantalla dice, ademas, que no se escucho nada).
 * Umbrales configurables (params): abierta >= open_thr, marginal >= marginal_thr. */
propagation_verdict_t propagation_verdict(int snr_db, bool heard, int open_thr, int marginal_thr);
const char *propagation_verdict_name(propagation_verdict_t v);

/* --- Escalera de potencia (medicion por ventana del slot) ----------------- */

int         propagation_ladder_steps(void);

/* Potencia del escalon `step` (0..3) en decimas de watt: 1000, 100, 10 y 1
 * (o sea 100 / 10 / 1 / 0.1 W). */
int         propagation_ladder_power_dw(int step);

/* Un escalon se da por escuchado cuando su ventana supera este piso sobre el
 * ruido (dB): por debajo no se distingue del piso. */
#define PROPAGATION_DETECT_DB 3

/* Ventana de cada escalon dentro del slot de 10 s (issue #145, seccion 5). El
 * beacon manda el indicativo y el primer dash a 100 W y despues tres dashes de
 * 1 s a 10, 1 y 0.1 W, que terminan con el slot. La v1 mide:
 *
 *   escalon 0 (100 W): los primeros 5 s (indicativo + primer dash)
 *   escalon 1 (10 W):  [5 s, 6 s)
 *   escalon 2 (1 W):   [7 s, 8 s)
 *   escalon 3 (0.1 W): [9 s, 10 s)
 *
 * Es una ventana FIJA: no se detectan los flancos del CW. Si la radio muestra
 * que el formato real esta corrido, se corrige aca y en el worker del dialogo;
 * con senal debil el escalon se cae solo (ver PROPAGATION_DETECT_DB). */
#define PROPAGATION_LADDER_SLOT_MS  10000
#define PROPAGATION_LADDER_WINDOW_MS 1000

/* Inicio y largo de la ventana del escalon `step` (0..3), en ms desde el inicio
 * del slot. False fuera de rango. */
bool        propagation_ladder_window(int step, int *start_ms, int *len_ms);

/* La escalera MEDIDA en un slot: `step_max_db[k]` es el maximo del S-meter en la
 * ventana k (ver propagation_ladder_window()) y `noise_db` el piso de ruido
 * medido en la misma banda. Devuelve cuantos escalones SEGUIDOS se escucharon
 * desde el primero (0..4: cada uno baja 10x la potencia, asi que uno debil visto
 * detras de un hueco no se cuenta) y deja el SNR de cada ventana en `snr_out`
 * (puede ser NULL). */
int         propagation_ladder_from_slot(const int step_max_db[PROPAGATION_LADDER_STEPS], int noise_db,
                                         int detect_db, int snr_out[PROPAGATION_LADDER_STEPS]);

/* --- Umbrales del veredicto (presets) ------------------------------------ */

/* Pares (abierta, marginal) en dB que el operador elige: un boton de la app y un
 * desplegable de Ajustes recorren esta tabla y guardan el INDICE (issue #145,
 * seccion 6: umbrales configurables y documentados). */
int         propagation_snr_preset_count(void);
bool        propagation_snr_preset(int i, int *open_db, int *marg_db);

/* Indice del preset con esos umbrales, o -1 si el par no esta en la tabla. */
int         propagation_snr_preset_index(int open_db, int marg_db);

/* --- Registro IARU Region 2 (10 m) -------------------------------------- */

/* Radiofaros de 10 m de la Region 2 (hojas ACTIVE e INactive de la lista oficial
 * de la IARU R2), empaquetados en el firmware: no hay red. Ver
 * src/propagation_data.c y tools/gen_propagation_iaru.py. Solo se usan como
 * referencia (identificar y contar los radiofaros registrados); el barrido mide
 * los beacons IBP, que son los que tienen turno. */
int         propagation_iaru_count(void);
int         propagation_iaru_active_count(void);

/* Registro `i`: indicativo, frecuencia (Hz) y si la lista lo da por activo. False
 * fuera de rango. */
bool        propagation_iaru_beacon(int i, const char **call, uint32_t *hz, bool *active);

/* Indicativo del radiofaro ACTIVO que esta exactamente en `hz`, o NULL. */
const char *propagation_iaru_find(uint32_t hz);

#ifdef __cplusplus
}
#endif

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Adaptive Tuning (issue #186)
 *
 *  Modulo PURO de C: no depende de LVGL ni de ningun reloj global. El reloj
 *  son los timestamps del kernel (struct input_event.time, microsegundos,
 *  CLOCK_MONOTONIC) que le pasa el productor de src/rotary.c; el estado vive
 *  SOLO en el hilo de la GUI.
 *
 *  El motor mide la velocidad REAL en detents/segundo (no cuantos eventos
 *  llegaron juntos a LVGL) y devuelve SIEMPRE uno de los multiplicadores
 *  enteros de la tabla del issue: nada de floats hacia la frecuencia.
 *
 *  Copyright (c) 2024 LU2DSX
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Umbral global `Accel threshold` (Settings -> Operation): detents/s de entrada
 * al nivel 1. Rango y default del contrato (seccion 2.2/2.3). */
#define ADAPTIVE_THRESHOLD_MIN      3
#define ADAPTIVE_THRESHOLD_MAX      15
#define ADAPTIVE_THRESHOLD_DEFAULT  7

/* Constantes del contrato (seccion 2.1). */
#define ADAPTIVE_PAUSE_US           180000u   /* freno: dt >= 180 ms => x1       */
#define ADAPTIVE_MIN_DT_US          1000u     /* dt_us minimo para computar      */
#define ADAPTIVE_MAX_SPEED_DPS      1000.0f   /* tope: 1e6/dt_us                 */
#define ADAPTIVE_SMOOTH_UP          0.35f     /* subida: 0.35v + 0.65s           */
#define ADAPTIVE_HYSTERESIS         0.80f     /* salida: < 0.80 x umbral         */
#define ADAPTIVE_LEVELS             4         /* niveles 1..4 (0 = x1)           */
#define ADAPTIVE_WARMUP_INTERVALS   2         /* arranque: nivel <= 1            */

/* Multiplicadores FIJOS de la tabla de la seccion 2.2. El indice es el nivel
 * 0..4. No se calculan con floats ni dependen del umbral. */
typedef enum {
    ADAPTIVE_ACCEL_NONE = 0,    /* siempre x1, no mantiene aceleracion */
    ADAPTIVE_ACCEL_LITE,        /* x1 x2 x3 x6 x10                     */
    ADAPTIVE_ACCEL_STRONG,      /* x1 x3 x6 x12 x24                    */
} adaptive_accel_t;

/* Estado del motor. Todo en el hilo de la GUI: el productor (hilo de lectura)
 * solo encola eventos con su timestamp. */
typedef struct {
    uint64_t    last_timestamp_us;  /* ultimo detent valido                   */
    bool        have_timestamp;     /* false hasta el primer detent            */
    int8_t      last_direction;     /* +1 / -1 / 0                             */
    float       smoothed_dps;       /* velocidad filtrada en detents/s         */
    bool        smoothed_valid;     /* false = todavia no hay valor filtrado   */
    uint8_t     level;              /* 0..4                                    */
    uint8_t     warmup;             /* intervalos validos desde el ultimo reset */
} adaptive_tuning_state_t;

/* Payload de EVENT_ROTARY_ENHANCED (seccion 4).
 *
 * PROPIEDAD SÍNCRONA: `lv_event_send()` entrega el parametro de forma
 * SINCRONICA, dentro de la misma llamada, asi que el emisor puede dejarlo en
 * el stack. El receptor NO debe hacer free() ni guardar el puntero. */
typedef struct {
    int32_t     raw_diff;           /* detents crudos del tramo de igual signo  */
    int64_t     weighted_detents;   /* suma de signo x multiplicador por detent */
    uint8_t     last_multiplier;    /* ultimo multiplicador entero aplicado     */
} rotary_event_enhanced_t;

/* Inicializa (equivale a reset). */
void adaptive_tuning_init(adaptive_tuning_state_t *state);

/* Descarta velocidad, nivel, direccion y timestamp: el proximo detent es x1. */
void adaptive_tuning_reset(adaptive_tuning_state_t *state);

/* Procesa UN detent (`direction` exactamente +1/-1) y devuelve el multiplicador
 * entero. `threshold_dps` se valida a 3..15 (fuera de rango => 7). */
uint8_t adaptive_tuning_detent(adaptive_tuning_state_t *state, int direction,
                               uint64_t timestamp_us, adaptive_accel_t profile,
                               uint8_t threshold_dps);

/* Umbrales derivados de T (seccion 2.2), aritmetica entera:
 *   out[0] = T, out[1] = ceil(15T/7), out[2] = ceil(30T/7), out[3] = ceil(50T/7).
 * `out` tiene que tener 4 elementos. */
void adaptive_tuning_thresholds(uint8_t threshold_dps, uint8_t out[ADAPTIVE_LEVELS]);

/* Multiplicador fijo del nivel 0..4 para el perfil dado. */
uint8_t adaptive_tuning_multiplier(adaptive_accel_t profile, uint8_t level);

/* Procesa un evento EV_REL completo (|diff| puede ser > 1): lo divide en detents
 * unitarios interpolando los tiempos uniformemente entre el timestamp del evento
 * anterior y el de este (seccion 2.1) y devuelve la suma ponderada con signo.
 * El primer evento sin timestamp anterior da todos sus detents x1; una marca
 * repetida usa la separacion virtual de 1000 us y no acelera. */
int64_t adaptive_tuning_weighted_event(adaptive_tuning_state_t *state, int32_t diff,
                                       uint64_t timestamp_us, adaptive_accel_t profile,
                                       uint8_t threshold_dps, uint8_t *last_multiplier);

/* Procesa `n` entradas de cola EN ORDEN y las agrupa en tramos de igual signo
 * (nunca netea signos contrarios). Una entrada con diff == 0 es una marca de
 * discontinuidad del reloj: resetea el motor y corta el tramo. Devuelve cuantos
 * tramos escribio en `out` (<= out_max). */
size_t adaptive_tuning_segments(adaptive_tuning_state_t *state,
                                const int32_t *diffs, const uint64_t *timestamps_us, size_t n,
                                adaptive_accel_t profile, uint8_t threshold_dps,
                                rotary_event_enhanced_t *out, size_t out_max);

/* Igual que adaptive_tuning_segments(), pero opcionalmente devuelve el
 * multiplicador del ULTIMO detent de cada evento de entrada (0 en una marca de
 * discontinuidad). Lo usa el test de batches para comparar la lista de
 * multiplicadores bit a bit; el runtime pasa NULL. */
size_t adaptive_tuning_segments_ex(adaptive_tuning_state_t *state,
                                   const int32_t *diffs, const uint64_t *timestamps_us, size_t n,
                                   adaptive_accel_t profile, uint8_t threshold_dps,
                                   rotary_event_enhanced_t *out, size_t out_max,
                                   uint8_t *event_multipliers, size_t event_multipliers_max);

/* Aritmetica segura del VFO (seccion 4): target = current + weighted * step en
 * int64, con verificacion de rango int32. Devuelve false (y no escribe `out`)
 * si el resultado desborda: quien llama NO debe tocar la frecuencia. */
bool adaptive_tuning_target_freq(int32_t current, int64_t weighted_detents,
                                 int32_t step, int32_t *out);

/* Saneamiento del parametro persistido: 0 o fuera de 3..15 => 7. */
uint8_t adaptive_tuning_threshold_sanitize(uint8_t threshold_dps);

#ifdef __cplusplus
}
#endif

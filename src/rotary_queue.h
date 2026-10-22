/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - cola del dial principal (issue #186, seccion 3)
 *
 *  Modulo PURO (solo POSIX): lo usa el hilo lector de src/rotary.c como
 *  productor y el hilo de la GUI como consumidor. Sin LVGL, sin Subjects y sin
 *  radio: se compila tal cual en tests/native/.
 *
 *  - Capacidad 512 entradas, mutex + condition variables.
 *  - Backpressure: cuando la cola esta llena el PRODUCTOR ESPERA espacio (no
 *    hay overwrite, ni descarte, ni spin) y el consumidor lo despierta despues
 *    de extraer.
 *  - `diff == 0` es una marca de discontinuidad del reloj (marca no creciente):
 *    el consumidor resetea el motor adaptativo. Nunca es un detent.
 *
 *  Copyright (c) 2024 LU2DSX
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROTARY_QUEUE_SIZE   512

/* Un detent tal como sale del kernel: cuanto se movio y SU timestamp. */
typedef struct {
    int32_t     diff;           /* EV_REL value (0 = discontinuidad de reloj) */
    uint64_t    timestamp_us;   /* struct input_event.time, microsegundos      */
} rotary_data_t;

typedef struct {
    rotary_data_t   q[ROTARY_QUEUE_SIZE];
    uint16_t        r;
    uint16_t        w;
    uint16_t        count;

    /* Diagnostico en debug: ocupacion pico y cuantas veces el productor
     * espero. Se leen con rotary_queue_stats(); nunca se imprime por detent. */
    uint32_t        high_watermark;
    uint32_t        queue_full_waits;

    pthread_mutex_t mux;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} rotary_queue_t;

void     rotary_queue_init(rotary_queue_t *q);

/* Encola un detent. Si la cola esta llena, BLOQUEA al productor hasta que haya
 * lugar (backpressure) y cuenta la espera en queue_full_waits. */
void     rotary_queue_push(rotary_queue_t *q, int32_t diff, uint64_t timestamp_us);

/* Extrae un detent en orden. Devuelve false si la cola esta vacia (no bloquea). */
bool     rotary_queue_pop(rotary_queue_t *q, rotary_data_t *out);

/* Extrae hasta `max` en orden; devuelve cuantos extrajo. */
size_t   rotary_queue_drain(rotary_queue_t *q, rotary_data_t *out, size_t max);

uint16_t rotary_queue_count(rotary_queue_t *q);
void     rotary_queue_stats(rotary_queue_t *q, uint32_t *high_watermark, uint32_t *queue_full_waits);

#ifdef __cplusplus
}
#endif

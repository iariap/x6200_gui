/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - cola del dial principal (issue #186, seccion 3)
 *
 *  Copyright (c) 2024 LU2DSX
 */

#include "rotary_queue.h"

#include <stddef.h>
#include <string.h>

void rotary_queue_init(rotary_queue_t *q) {
    if (q == NULL) {
        return;
    }

    memset(q, 0, sizeof(*q));

    pthread_mutex_init(&q->mux, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void rotary_queue_push(rotary_queue_t *q, int32_t diff, uint64_t timestamp_us) {
    if (q == NULL) {
        return;
    }

    pthread_mutex_lock(&q->mux);

    /* Backpressure: no se descarta nada. El productor espera al consumidor. */
    while (q->count == ROTARY_QUEUE_SIZE) {
        q->queue_full_waits++;
        pthread_cond_wait(&q->not_full, &q->mux);
    }

    q->q[q->w].diff         = diff;
    q->q[q->w].timestamp_us = timestamp_us;
    q->w = (uint16_t) ((q->w + 1) % ROTARY_QUEUE_SIZE);
    q->count++;

    if (q->count > q->high_watermark) {
        q->high_watermark = q->count;
    }

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mux);
}

bool rotary_queue_pop(rotary_queue_t *q, rotary_data_t *out) {
    bool got = false;

    if (q == NULL || out == NULL) {
        return false;
    }

    pthread_mutex_lock(&q->mux);

    if (q->count > 0) {
        *out   = q->q[q->r];
        q->r   = (uint16_t) ((q->r + 1) % ROTARY_QUEUE_SIZE);
        q->count--;
        got = true;

        /* Despierta al productor que este esperando lugar. */
        pthread_cond_signal(&q->not_full);
    }

    pthread_mutex_unlock(&q->mux);

    return got;
}

size_t rotary_queue_drain(rotary_queue_t *q, rotary_data_t *out, size_t max) {
    size_t n = 0;

    if (q == NULL || out == NULL) {
        return 0;
    }

    pthread_mutex_lock(&q->mux);

    while (n < max && q->count > 0) {
        out[n++] = q->q[q->r];
        q->r     = (uint16_t) ((q->r + 1) % ROTARY_QUEUE_SIZE);
        q->count--;
    }

    if (n > 0) {
        pthread_cond_signal(&q->not_full);
    }

    pthread_mutex_unlock(&q->mux);

    return n;
}

uint16_t rotary_queue_count(rotary_queue_t *q) {
    uint16_t n;

    if (q == NULL) {
        return 0;
    }

    pthread_mutex_lock(&q->mux);
    n = q->count;
    pthread_mutex_unlock(&q->mux);

    return n;
}

void rotary_queue_stats(rotary_queue_t *q, uint32_t *high_watermark, uint32_t *queue_full_waits) {
    if (q == NULL) {
        return;
    }

    pthread_mutex_lock(&q->mux);

    if (high_watermark != NULL) {
        *high_watermark = q->high_watermark;
    }

    if (queue_full_waits != NULL) {
        *queue_full_waits = q->queue_full_waits;
    }

    pthread_mutex_unlock(&q->mux);
}

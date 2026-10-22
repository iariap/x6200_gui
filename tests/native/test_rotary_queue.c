/*
 * Unit tests: cola del dial principal (issue #186, seccion 3).
 *
 * Cubre FIFO, drenaje por lotes, wrap de los indices, preservacion del
 * timestamp, marca de discontinuidad (diff 0) y backpressure real con 512+
 * eventos concurrentes: el productor espera, no descarta ni duplica.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "rotary_queue.h"
#include "test_framework.h"

typedef struct {
    rotary_queue_t *q;
    int             count;
} producer_arg_t;

static void *producer_thread(void *arg) {
    producer_arg_t *p = (producer_arg_t *) arg;

    for (int i = 0; i < p->count; i++) {
        rotary_queue_push(p->q, +1, (uint64_t) (i + 1) * 10000ULL);
    }

    return NULL;
}

static void test_queue_fifo_and_drain(void) {
    rotary_queue_t q;
    rotary_data_t  d;

    rotary_queue_init(&q);

    CHECK(rotary_queue_count(&q) == 0);
    CHECK(!rotary_queue_pop(&q, &d));

    rotary_queue_push(&q, +1, 1000);
    rotary_queue_push(&q, -1, 2000);
    rotary_queue_push(&q, 0, 3000);      /* discontinuidad de reloj */

    CHECK(rotary_queue_count(&q) == 3);

    CHECK(rotary_queue_pop(&q, &d));
    CHECK(d.diff == +1);
    CHECK(d.timestamp_us == 1000);

    rotary_data_t out[4];
    size_t        n = rotary_queue_drain(&q, out, 4);

    CHECK(n == 2);
    CHECK(out[0].diff == -1);
    CHECK(out[0].timestamp_us == 2000);
    CHECK(out[1].diff == 0);
    CHECK(out[1].timestamp_us == 3000);
    CHECK(rotary_queue_count(&q) == 0);
}

static void test_queue_drain_limit(void) {
    rotary_queue_t q;

    rotary_queue_init(&q);

    for (int i = 0; i < 10; i++) {
        rotary_queue_push(&q, +1, (uint64_t) i);
    }

    rotary_data_t out[4];

    CHECK(rotary_queue_drain(&q, out, 4) == 4);
    CHECK(out[0].timestamp_us == 0);
    CHECK(out[3].timestamp_us == 3);
    CHECK(rotary_queue_count(&q) == 6);

    CHECK(rotary_queue_drain(&q, out, 4) == 4);
    CHECK(out[0].timestamp_us == 4);
    CHECK(out[3].timestamp_us == 7);
    CHECK(rotary_queue_count(&q) == 2);

    CHECK(rotary_queue_drain(&q, out, 4) == 2);
    CHECK(out[0].timestamp_us == 8);
    CHECK(out[1].timestamp_us == 9);
    CHECK(rotary_queue_count(&q) == 0);
    CHECK(rotary_queue_drain(&q, out, 4) == 0);
}

static void test_queue_wrap(void) {
    rotary_queue_t q;

    rotary_queue_init(&q);

    /* Mas de dos vueltas completas de 512: los indices tienen que envolver sin
     * corromper el orden. */
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < 400; i++) {
            rotary_queue_push(&q, (cycle * 400 + i), (uint64_t) (cycle * 400 + i));
        }

        for (int i = 0; i < 400; i++) {
            rotary_data_t d;
            int           expected = cycle * 400 + i;

            CHECK(rotary_queue_pop(&q, &d));
            CHECK(d.diff == expected);
            CHECK(d.timestamp_us == (uint64_t) expected);
        }
    }

    CHECK(rotary_queue_count(&q) == 0);
}

static void test_queue_backpressure_512(void) {
    rotary_queue_t q;
    producer_arg_t pa;
    pthread_t      th;
    int            received = 0;
    int            last     = -1;
    bool           order_ok = true;
    uint32_t       high = 0, waits = 0;

    rotary_queue_init(&q);

    pa.q     = &q;
    pa.count = 2000;

    CHECK(pthread_create(&th, NULL, producer_thread, &pa) == 0);

    /* El consumidor no arranca todavia: la cola se llena y el productor queda
     * bloqueado en push (no hay overwrite ni descarte). */
    usleep(20000);
    CHECK(rotary_queue_count(&q) == ROTARY_QUEUE_SIZE);

    while (received < pa.count) {
        rotary_data_t d;

        if (!rotary_queue_pop(&q, &d)) {
            usleep(1000);
            continue;
        }

        int index = (int) (d.timestamp_us / 10000ULL) - 1;

        if (d.diff != +1 || index != last + 1) {
            order_ok = false;
        }

        last = index;
        received++;
    }

    CHECK(pthread_join(th, NULL) == 0);

    rotary_queue_stats(&q, &high, &waits);

    CHECK(order_ok);
    CHECK(received == 2000);            /* ninguno omitido ni duplicado */
    CHECK(high == ROTARY_QUEUE_SIZE);
    CHECK(waits > 0);
    CHECK(rotary_queue_count(&q) == 0);
}

void test_rotary_queue(void) {
    test_queue_fifo_and_drain();
    test_queue_drain_limit();
    test_queue_wrap();
    test_queue_backpressure_512();
}

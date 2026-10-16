/*
 * Native test for the event queue (src/events.c), issue #111: "la cola de
 * eventos se satura y tira eventos".
 *
 * The firmware queue is a single-producer/single-consumer ring guarded by a
 * mutex: producers (radio thread, message senders, timers) push, the LVGL loop
 * drains in event_obj_check(). Two defects are pinned here:
 *
 *   1. the ring laps its reader: when the queue fills, the producer writes into
 *      the slot the consumer uses as its "empty" marker (queue_read), leaving
 *      queue_write == queue_read: the consumer then sees the queue as empty and
 *      every stranded item is lost while all later sends are dropped forever;
 *   2. the drop path logs one line per drop, so a saturated queue writes the
 *      SD ~2.4 lines/s non-stop and buries the rest of the log.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "events.h"
#include "events_stubs.h"

#include "test_framework.h"

/* El tope de lineas de detalle del log de overflow lo define src/events.c; el
 * fallback permite compilar el test contra la version previa al fix (RED). */
#ifndef EVENTS_OVERFLOW_LOG_DETAIL
#define EVENTS_OVERFLOW_LOG_DETAIL 8
#endif

/* Capacidad util de la cola: QUEUE_SIZE (64) menos el slot de marca de vacio. */
#define QUEUE_CAPACITY 63

static void test_queue_does_not_lap_its_reader(void) {
    events_stub_reset();
    event_init();

    /* 63 llenan la cola; el envio 64 tiene que descartarse limpio, sin pisar el
     * slot de queue_read. Con el defecto, los 64 eventos quedan varados y el
     * consumidor ve la cola vacia. */
    for (int i = 0; i < QUEUE_CAPACITY + 1; i++) {
        event_send(NULL, LV_EVENT_KEY, NULL);
    }

    event_obj_check();

    CHECK(events_stub_dispatched() == QUEUE_CAPACITY);

    /* Y la cola queda usable: el evento siguiente entra y se procesa. */
    event_send(NULL, LV_EVENT_KEY, NULL);
    event_obj_check();

    CHECK(events_stub_dispatched() == QUEUE_CAPACITY + 1);
}

static void test_overflow_log_is_rate_limited(void) {
    events_stub_reset();
    event_init();
    events_stub_tick_set(1000);

    for (int i = 0; i < QUEUE_CAPACITY; i++) {
        event_send(NULL, LV_EVENT_KEY, NULL);
    }

    int before = events_stub_log_lines();

    for (int i = 0; i < 100; i++) {
        event_send(NULL, LV_EVENT_KEY, NULL);
    }

    int lines = events_stub_log_lines() - before;

    /* 100 descartes con el reloj quieto no pueden producir 100 lineas: a lo sumo
     * los EVENTS_OVERFLOW_LOG_DETAIL primeros salen en detalle. */
    CHECK(lines <= EVENTS_OVERFLOW_LOG_DETAIL);
}

void test_events(void) {
    test_queue_does_not_lap_its_reader();
    test_overflow_log_is_rate_limited();
}

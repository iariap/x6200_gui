/*
 * Native test for the event queue (src/events.c), issue #111: "la cola de
 * eventos se satura y tira eventos".
 *
 * The firmware queue is a single-producer/single-consumer ring guarded by a
 * mutex: producers (radio thread, message senders, timers) push, the LVGL loop
 * drains in event_obj_check(). The defects pinned here:
 *
 *   1. the ring laps its reader: when the queue fills, the producer wrote into
 *      the slot the consumer uses as its "empty" marker (queue_read), leaving
 *      queue_write == queue_read: the consumer then sees the queue as empty and
 *      every stranded item is lost while all later sends are dropped forever;
 *   2. the drop path logged one line per drop, so a saturated queue wrote the
 *      SD ~2.4 lines/s non-stop and buried the rest of the log;
 *   3. a dropped event leaked its param (it is never enqueued, so the consumer
 *      never frees it) — caught by ASan/LeakSanitizer when available.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "events_stubs.h"

#include "test_framework.h"

/* Los topes del log de overflow los define src/events.c; los fallbacks permiten
 * compilar el test contra la version previa al fix (RED). */
#ifndef EVENTS_OVERFLOW_LOG_DETAIL
#define EVENTS_OVERFLOW_LOG_DETAIL 8
#endif

#ifndef EVENTS_OVERFLOW_LOG_PERIOD_MS
#define EVENTS_OVERFLOW_LOG_PERIOD_MS 10000
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

static void test_overflow_log_summary_is_periodic(void) {
    events_stub_reset();
    event_init();
    events_stub_tick_set(0);

    for (int i = 0; i < QUEUE_CAPACITY; i++) {
        event_send(NULL, EVENT_KEYPAD, NULL);
    }

    /* Los primeros descartes salen en detalle y nombran el tipo. */
    event_send(NULL, EVENT_KEYPAD, NULL);
    CHECK(events_stub_log_lines() == 1);
    CHECK(strstr(events_stub_last_log(), "keypad") != NULL);

    for (int i = 1; i < EVENTS_OVERFLOW_LOG_DETAIL; i++) {
        event_send(NULL, EVENT_KEYPAD, NULL);
    }

    int after_detail = events_stub_log_lines();
    CHECK(after_detail == EVENTS_OVERFLOW_LOG_DETAIL);

    /* Con el reloj quieto, mas descartes no agregan lineas. */
    for (int i = 0; i < 20; i++) {
        event_send(NULL, EVENT_KEYPAD, NULL);
    }
    CHECK(events_stub_log_lines() == after_detail);

    /* Pasado el periodo, un descarte mas emite UN resumen con el total y el
     * desglose por tipo. */
    events_stub_tick_set(EVENTS_OVERFLOW_LOG_PERIOD_MS);
    event_send(NULL, EVENT_KEYPAD, NULL);

    CHECK(events_stub_log_lines() == after_detail + 1);

    event_queue_stats_t st;
    event_queue_stats_get(&st);

    char want[32];
    snprintf(want, sizeof(want), "keypad=%u", (unsigned) st.dropped);

    CHECK(strstr(events_stub_last_log(), "eventos perdidos") != NULL);
    CHECK(strstr(events_stub_last_log(), want) != NULL);
    CHECK(strstr(events_stub_last_log(), "pico cola") != NULL);
}

static void test_drop_counters_by_type(void) {
    events_stub_reset();
    event_init();
    events_stub_tick_set(0);

    for (int i = 0; i < QUEUE_CAPACITY; i++) {
        event_send(NULL, EVENT_KEYPAD, NULL);
    }
    for (int i = 0; i < 10; i++) {
        event_send(NULL, EVENT_KEYPAD, NULL);
    }
    for (int i = 0; i < 5; i++) {
        event_send(NULL, EVENT_MSG_UPDATE, NULL);
    }

    event_queue_stats_t st;
    event_queue_stats_get(&st);

    CHECK(st.dropped == 15);
    CHECK(st.pending == QUEUE_CAPACITY);
    CHECK(st.peak == QUEUE_CAPACITY);
    CHECK(st.capacity == QUEUE_CAPACITY);
    CHECK(event_queue_drops_of(EVENT_KEYPAD) == 10);
    CHECK(event_queue_drops_of(EVENT_MSG_UPDATE) == 5);
    CHECK(event_queue_drops_of(EVENT_HKEY) == 0);
}

static void test_dropped_event_frees_its_param(void) {
    events_stub_reset();
    event_init();

    for (int i = 0; i < QUEUE_CAPACITY; i++) {
        event_send(NULL, EVENT_KEYPAD, NULL);
    }

    /* Un evento descartado no se encola, asi que su param lo tiene que liberar
     * event_send(): si no, cada drop filtra memoria. Lo verifica ASan. */
    int *p = malloc(sizeof(int));
    *p = 42;

    event_send(NULL, EVENT_KEYPAD, (void *) p);

    CHECK(event_queue_drops_of(EVENT_KEYPAD) == 1);
}

void test_events(void) {
    test_queue_does_not_lap_its_reader();
    test_overflow_log_is_rate_limited();
    test_overflow_log_summary_is_periodic();
    test_drop_counters_by_type();
    test_dropped_event_frees_its_param();
}

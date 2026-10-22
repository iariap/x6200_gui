/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include "events.h"
#include "backlight.h"
#include "keyboard.h"

#define QUEUE_SIZE  64

/* Capacidad util: el anillo reserva un slot como marca de vacio (queue_read),
 * asi que entran QUEUE_SIZE - 1 eventos. */
#define QUEUE_CAPACITY  (QUEUE_SIZE - 1)

/* Log del descarte (#111): los primeros EVENTS_OVERFLOW_LOG_DETAIL drops salen
 * en detalle (que tipo de evento se perdio y por que) y despues, a lo sumo un
 * resumen cada EVENTS_OVERFLOW_LOG_PERIOD_MS con el total y el desglose por
 * tipo. Sin esto, una cola saturada escribe una linea por drop (medido: ~2,4
 * lineas/s sostenidas, el 97% del log) y tapa el resto de los avisos. */
#define EVENTS_OVERFLOW_LOG_DETAIL     8
#define EVENTS_OVERFLOW_LOG_PERIOD_MS  10000

/* Cuantos tipos de evento distintos se desglosan antes de mandar el resto al
 * bucket "otros": los ids de evento son dinamicos (se registran al arrancar). */
#define EVENTS_DROP_CODES  16

uint32_t        EVENT_ROTARY_ENHANCED;
uint32_t        EVENT_KEYPAD;
uint32_t        EVENT_HKEY;
uint32_t        EVENT_RADIO_TX;
uint32_t        EVENT_RADIO_RX;
uint32_t        EVENT_SCREEN_UPDATE;
uint32_t        EVENT_MSG_UPDATE;
uint32_t        EVENT_BAND_UP;
uint32_t        EVENT_BAND_DOWN;

typedef struct {
    lv_obj_t        *obj;
    lv_event_code_t event_code;
    void            *param;
} item_t;

static item_t           *queue[QUEUE_SIZE];
static uint8_t          queue_write = 0;
static uint8_t          queue_read = 0;
static pthread_mutex_t  queue_mux;

/* --- Instrumentacion (#111) --- */

static uint32_t         queue_pending = 0;
static uint32_t         queue_peak = 0;
static uint32_t         dropped_total = 0;

typedef struct {
    uint32_t    code;
    uint32_t    drops;
} drop_entry_t;

static drop_entry_t     drop_by_code[EVENTS_DROP_CODES];
static uint32_t         drop_by_code_n = 0;
static uint32_t         drop_other = 0;

static uint32_t         overflow_log_detail = 0;
static uint32_t         overflow_log_tick = 0;

/* Ultimo evento PROCESADO (no encolado): el registro de ultima respiracion
 * (issue #121) lo escribe al lado del estado de la GUI. Es un entero alineado:
 * el productor y el consumidor no necesitan el mutex de la cola para leerlo. */
static volatile uint32_t last_event_code = 0;

void event_queue_stats_reset(void) {
    queue_peak = 0;
    dropped_total = 0;
    drop_by_code_n = 0;
    drop_other = 0;
    overflow_log_detail = 0;
    overflow_log_tick = 0;
}

void event_init() {
    EVENT_ROTARY_ENHANCED = lv_event_register_id();
    EVENT_KEYPAD = lv_event_register_id();
    EVENT_HKEY = lv_event_register_id();
    EVENT_RADIO_TX = lv_event_register_id();
    EVENT_RADIO_RX = lv_event_register_id();
    EVENT_SCREEN_UPDATE = lv_event_register_id();
    EVENT_MSG_UPDATE = lv_event_register_id();
    EVENT_BAND_UP = lv_event_register_id();
    EVENT_BAND_DOWN = lv_event_register_id();

    for (uint8_t i = 0; i < QUEUE_SIZE; i++)
        queue[i] = NULL;

    queue_write = 0;
    queue_read = 0;
    queue_pending = 0;
    last_event_code = 0;

    event_queue_stats_reset();

    pthread_mutex_init(&queue_mux, NULL);
}

void event_obj_check() {
    for (;;) {
        pthread_mutex_lock(&queue_mux);

        if (queue_read == queue_write) {
            pthread_mutex_unlock(&queue_mux);
            break;
        }

        queue_read = (queue_read + 1) % QUEUE_SIZE;

        item_t *item = queue[queue_read];
        queue[queue_read] = NULL;

        if (queue_pending > 0) {
            queue_pending--;
        }

        pthread_mutex_unlock(&queue_mux);

        if (item) {
            last_event_code = item->event_code;

            if (!item->obj) {
                item->obj = lv_scr_act();
            }
            if (item->event_code == LV_EVENT_REFRESH) {
                lv_obj_invalidate(item->obj);
            } else {
                lv_event_send(item->obj, item->event_code, item->param);
            }

            if (item->param != NULL) {
                free(item->param);
            }

            free(item);
        }
    }
}

/* Nombre corto del tipo de evento, para el log: es el codigo (categoria), nunca
 * el contenido del evento. */
static const char *event_code_name(uint32_t code) {
    if (code == EVENT_ROTARY_ENHANCED) return "rotary";
    if (code == EVENT_KEYPAD)          return "keypad";
    if (code == EVENT_HKEY)            return "hkey";
    if (code == EVENT_RADIO_TX)        return "radio-tx";
    if (code == EVENT_RADIO_RX)        return "radio-rx";
    if (code == EVENT_SCREEN_UPDATE)   return "screen-update";
    if (code == EVENT_MSG_UPDATE)      return "msg-update";
    if (code == EVENT_BAND_UP)         return "band-up";
    if (code == EVENT_BAND_DOWN)       return "band-down";
    if (code == LV_EVENT_REFRESH)      return "refresh";
    if (code == LV_EVENT_KEY)          return "key";

    return "otro";
}

uint32_t event_last_code(void) {
    return last_event_code;
}

const char *event_last_name(void) {
    if (last_event_code == 0) {
        return "none";
    }

    return event_code_name(last_event_code);
}

static void note_drop(uint32_t code) {
    dropped_total++;

    for (uint32_t i = 0; i < drop_by_code_n; i++) {
        if (drop_by_code[i].code == code) {
            drop_by_code[i].drops++;
            return;
        }
    }

    if (drop_by_code_n < EVENTS_DROP_CODES) {
        drop_by_code[drop_by_code_n].code = code;
        drop_by_code[drop_by_code_n].drops = 1;
        drop_by_code_n++;
    } else {
        drop_other++;
    }
}

/* Anexa a buf con tope: devuelve la posicion siguiente (puede superar cap; en
 * ese caso el append no escribe y vsnprintf ya dejo el string terminado). */
static int append(char *buf, size_t cap, int pos, const char *fmt, ...) {
    if (pos < 0 || (size_t) pos >= cap) {
        return pos;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf + pos, cap - (size_t) pos, fmt, args);
    va_end(args);

    if (written < 0) {
        return pos;
    }

    return pos + written;
}

static void discard_log_summary(void) {
    char buf[192];
    int  pos = 0;

    pos = append(buf, sizeof(buf), pos, "Overflow: %u eventos perdidos", (unsigned) dropped_total);

    for (uint32_t i = 0; i < drop_by_code_n; i++) {
        pos = append(buf, sizeof(buf), pos, "; %s=%u",
                     event_code_name(drop_by_code[i].code), (unsigned) drop_by_code[i].drops);
    }

    if (drop_other > 0) {
        pos = append(buf, sizeof(buf), pos, "; otros=%u", (unsigned) drop_other);
    }

    append(buf, sizeof(buf), pos, "; pico cola %u/%u", (unsigned) queue_peak, (unsigned) QUEUE_CAPACITY);

    LV_LOG_ERROR("%s", buf);
}

/* Log acotado del descarte: los primeros EVENTS_OVERFLOW_LOG_DETAIL en detalle,
 * despues a lo sumo un resumen cada EVENTS_OVERFLOW_LOG_PERIOD_MS. */
static void discard_log(uint32_t code, bool oom) {
    uint32_t t = lv_tick_get();

    if (overflow_log_detail < EVENTS_OVERFLOW_LOG_DETAIL) {
        overflow_log_detail++;
        overflow_log_tick = t;

        if (oom) {
            LV_LOG_ERROR("Sin memoria: se descarta %s (perdidos %u)", event_code_name(code),
                         (unsigned) dropped_total);
        } else {
            LV_LOG_ERROR("Overflow: se descarta %s (perdidos %u)", event_code_name(code),
                         (unsigned) dropped_total);
        }
        return;
    }

    if ((uint32_t) (t - overflow_log_tick) >= EVENTS_OVERFLOW_LOG_PERIOD_MS) {
        overflow_log_tick = t;
        discard_log_summary();
    }
}

/* Camino comun del descarte: cuenta, loguea acotado y libera el param. Se llama
 * SIEMPRE con el mutex suelto: escribir a la SD no puede frenar a los
 * productores. */
static void discard_event(uint32_t code, void *param, bool oom) {
    note_drop(code);
    discard_log(code, oom);

    if (param != NULL) {
        free(param);
    }
}

void event_send(lv_obj_t *obj, lv_event_code_t event_code, void *param) {
    pthread_mutex_lock(&queue_mux);

    uint8_t next = (queue_write + 1) % QUEUE_SIZE;

    /* La cola esta llena cuando el slot siguiente es el que el consumidor usa
     * como marca de vacio (queue_read): si se escribiera ahi, queue_write
     * quedaria igual a queue_read y el consumidor veria la cola vacia con todo
     * adentro. La deteccion vieja por contenido (queue[next] != NULL) fallaba
     * justo en ese borde: aceptaba el evento que pisaba la marca y trababa la
     * cola para siempre (issue #111). */
    if (next == queue_read) {
        pthread_mutex_unlock(&queue_mux);
        discard_event(event_code, param, false);
        return;
    }

    item_t *item = malloc(sizeof(item_t));

    if (!item) {
        /* Sin memoria tampoco hay evento que encolar: se cuenta como perdido en
         * vez de desreferenciar NULL (lo ve gcc -fanalyzer). */
        pthread_mutex_unlock(&queue_mux);
        discard_event(event_code, param, true);
        return;
    }

    item->obj = obj;
    item->event_code = event_code;
    item->param = param;

    queue[next] = item;
    queue_write = next;

    if (++queue_pending > queue_peak) {
        queue_peak = queue_pending;
    }

    pthread_mutex_unlock(&queue_mux);
}

uint32_t event_queue_drops_of(uint32_t event_code) {
    for (uint32_t i = 0; i < drop_by_code_n; i++) {
        if (drop_by_code[i].code == event_code) {
            return drop_by_code[i].drops;
        }
    }

    return 0;
}

void event_queue_stats_get(event_queue_stats_t *out) {
    if (!out) {
        return;
    }

    out->pending = queue_pending;
    out->peak = queue_peak;
    out->capacity = QUEUE_CAPACITY;
    out->dropped = dropped_total;
}

void event_send_key(int32_t key) {
    int32_t *c = malloc(sizeof(int32_t));

    if (!c) {
        /* Sin memoria no hay evento de tecla que encolar (gcc -fanalyzer). */
        discard_event(LV_EVENT_KEY, NULL, true);
        return;
    }

    *c = key;

    event_send(lv_group_get_focused(keyboard_group), LV_EVENT_KEY, c);
}

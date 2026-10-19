/*
 * Capturing stubs for the native hkey test (issue #131).
 *
 * Ver tests/native/test_hkey.c y el bloque "hkey" de
 * tests/native/run_native_tests.sh: el modulo real src/hkey.c corre contra este
 * arbol de stubs y el test fija (a) que hkey_put() puede llamarse desde
 * cualquier hilo sin tocar LVGL y (b) que la maquina de estados de la tecla
 * (press / long / release, teclas numericas) no cambia.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hkey_stubs.h"
#include "scheduler.h"

#include <stdlib.h>
#include <string.h>

/* --- estado del test ------------------------------------------------------- */

static pthread_t    lvgl_tid;
static bool         lvgl_tid_set;

static unsigned     lvgl_calls;
static unsigned     lvgl_foreign_calls;
static unsigned     backlight_ticks;
static unsigned     backlight_foreign;

typedef struct {
    event_hkey_t    ev;
} event_rec_t;

#define MAX_EVENTS 16

static event_rec_t  events[MAX_EVENTS];
static unsigned     events_n;
static unsigned     events_foreign;

static unsigned     keys_n;
static unsigned     keys_foreign;
static int32_t      last_key;

/* --- temporizadores -------------------------------------------------------- */

#define MAX_TIMERS 4

static lv_timer_t   timers[MAX_TIMERS];
static bool         timer_live[MAX_TIMERS];
static unsigned     timers_created;
static unsigned     timers_deleted;
static unsigned     timers_double_del;
static unsigned     timer_last;
static uint32_t     last_timer_period;
static int32_t      last_timer_repeat;

/* --- grupo de teclado ------------------------------------------------------ */

static lv_obj_t     screen;
static lv_obj_t     focused;
static lv_group_t   group;

lv_group_t *keyboard_group = &group;

/* --- puntuacion de hilos --------------------------------------------------- */

static void note_lvgl(void) {
    lvgl_calls++;
    if (lvgl_tid_set && !pthread_equal(pthread_self(), lvgl_tid)) {
        lvgl_foreign_calls++;
    }
}

void hkey_stub_reset(void) {
    lvgl_calls = 0;
    lvgl_foreign_calls = 0;
    backlight_ticks = 0;
    backlight_foreign = 0;

    memset(events, 0, sizeof(events));
    events_n = 0;
    events_foreign = 0;

    keys_n = 0;
    keys_foreign = 0;
    last_key = 0;

    memset(timers, 0, sizeof(timers));
    memset(timer_live, 0, sizeof(timer_live));
    timers_created = 0;
    timers_deleted = 0;
    timers_double_del = 0;
    timer_last = 0;
    last_timer_period = 0;
    last_timer_repeat = 0;

    group.editing = false;
    group.focused = &focused;
}

void hkey_stub_lvgl_thread(pthread_t tid) {
    lvgl_tid = tid;
    lvgl_tid_set = true;
}

unsigned hkey_stub_lvgl_calls(void)          { return lvgl_calls; }
unsigned hkey_stub_lvgl_foreign_calls(void)  { return lvgl_foreign_calls; }
unsigned hkey_stub_backlight_ticks(void)     { return backlight_ticks; }
unsigned hkey_stub_backlight_foreign(void)   { return backlight_foreign; }

unsigned hkey_stub_events(void)              { return events_n; }
unsigned hkey_stub_events_foreign(void)      { return events_foreign; }

event_hkey_t hkey_stub_last_event(void) {
    if (events_n == 0) {
        return (event_hkey_t) { .key = HKEY_UNKNOWN, .state = HKEY_RELEASE };
    }
    return events[events_n - 1].ev;
}

unsigned hkey_stub_keys(void)                { return keys_n; }
unsigned hkey_stub_keys_foreign(void)        { return keys_foreign; }
int32_t hkey_stub_last_key(void)             { return last_key; }

void hkey_stub_set_editing(bool editing)     { group.editing = editing; }

unsigned hkey_stub_timers_created(void)      { return timers_created; }
unsigned hkey_stub_timers_deleted(void)      { return timers_deleted; }
unsigned hkey_stub_timers_double_del(void)   { return timers_double_del; }
uint32_t hkey_stub_last_timer_period(void)   { return last_timer_period; }
int32_t hkey_stub_last_timer_repeat(void)    { return last_timer_repeat; }

unsigned hkey_stub_timers_live(void) {
    unsigned n = 0;

    for (unsigned i = 0; i < MAX_TIMERS; i++) {
        if (timer_live[i]) {
            n++;
        }
    }

    return n;
}

/* --- LVGL ----------------------------------------------------------------- */

lv_obj_t *lv_scr_act(void) {
    note_lvgl();
    return &screen;
}

bool lv_group_get_editing(const lv_group_t *g) {
    note_lvgl();
    return g->editing;
}

lv_obj_t *lv_group_get_focused(const lv_group_t *g) {
    note_lvgl();
    return g->focused;
}

lv_timer_t *lv_timer_create(lv_timer_cb_t timer_cb, uint32_t period, void *user_data) {
    note_lvgl();

    for (unsigned i = 0; i < MAX_TIMERS; i++) {
        if (!timer_live[i]) {
            timers[i] = (lv_timer_t) {
                .timer_cb     = timer_cb,
                .period       = period,
                .repeat_count = 0,
                .user_data    = user_data,
            };
            timer_live[i] = true;
            timers_created++;
            timer_last = i;
            last_timer_period = period;
            return &timers[i];
        }
    }

    return NULL;
}

void lv_timer_del(lv_timer_t *timer) {
    note_lvgl();

    for (unsigned i = 0; i < MAX_TIMERS; i++) {
        if (&timers[i] == timer) {
            if (!timer_live[i]) {
                timers_double_del++;
                return;
            }
            timer_live[i] = false;
            timers_deleted++;
            return;
        }
    }
}

void lv_timer_set_repeat_count(lv_timer_t *timer, int32_t repeat_count) {
    note_lvgl();
    timer->repeat_count = repeat_count;
    last_timer_repeat = repeat_count;
}

/* Corre el callback del ultimo temporizador armado, como haria
 * lv_timer_handler(). Un timer con repeat_count == 1 se libera al ejecutarse:
 * por eso se lo marca muerto ANTES de llamar al callback (hkey_timer limpia su
 * propio puntero). */
bool hkey_stub_timer_fire(void) {
    for (unsigned i = 0; i < MAX_TIMERS; i++) {
        if (timer_live[i] && i == timer_last) {
            lv_timer_cb_t cb = timers[i].timer_cb;

            timer_live[i] = false;
            timers_deleted++;

            cb(&timers[i]);
            return true;
        }
    }

    return false;
}

/* --- eventos (la cola real vive en src/events.c) --------------------------- */

uint32_t EVENT_HKEY;

void event_send(lv_obj_t *obj, lv_event_code_t event_code, void *param) {
    (void) obj;

    note_lvgl();

    if (event_code == EVENT_HKEY && param != NULL && events_n < MAX_EVENTS) {
        events[events_n].ev = *(event_hkey_t *) param;
        events_n++;
        if (lvgl_tid_set && !pthread_equal(pthread_self(), lvgl_tid)) {
            events_foreign++;
        }
    }

    /* La cola real (src/events.c, event_obj_check) es duena del param y lo
     * libera despues de despachar: el test hace lo mismo para que ASan/LSan
     * mire lo que mira la radio. */
    free(param);
}

void event_send_key(int32_t key) {
    note_lvgl();

    keys_n++;
    last_key = key;
    if (lvgl_tid_set && !pthread_equal(pthread_self(), lvgl_tid)) {
        keys_foreign++;
    }
}

/* --- backlight -------------------------------------------------------------- */

void backlight_tick(void) {
    note_lvgl();

    backlight_ticks++;
    if (lvgl_tid_set && !pthread_equal(pthread_self(), lvgl_tid)) {
        backlight_foreign++;
    }
}

/* --- scheduler falso ------------------------------------------------------- */

#define SCHED_MAX 64

typedef struct {
    void (*fn)(void *);
    void *arg;
} sched_item_t;

static sched_item_t sched_q[SCHED_MAX];
static unsigned     sched_read;
static unsigned     sched_write;

bool scheduler_put(scheduler_fn_t fn, void *arg, size_t arg_size) {
    if ((sched_write + 1) % SCHED_MAX == sched_read || fn == NULL) {
        return false;
    }

    void *copy = NULL;

    if (arg_size > 0) {
        copy = malloc(arg_size);
        if (copy == NULL) {
            return false;
        }
        memcpy(copy, arg, arg_size);
    }

    sched_q[sched_write].fn  = fn;
    sched_q[sched_write].arg = copy;
    sched_write = (sched_write + 1) % SCHED_MAX;
    return true;
}

bool scheduler_put_noargs(scheduler_fn_t fn) {
    return scheduler_put(fn, NULL, 0);
}

void scheduler_work(void) {
    while (sched_read != sched_write) {
        sched_item_t item = sched_q[sched_read];

        sched_read = (sched_read + 1) % SCHED_MAX;

        item.fn(item.arg);
        free(item.arg);
    }
}

unsigned scheduler_pending(void) {
    return (sched_write + SCHED_MAX - sched_read) % SCHED_MAX;
}

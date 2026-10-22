/*
 * Capturing stubs for the native cw_tune_ui test (issue #131).
 *
 * Ver tests/native/test_cw_tune.c y el bloque "cw_tune" de
 * tests/native/run_native_tests.sh: el modulo real src/cw_tune_ui.c corre contra
 * este arbol de stubs y el test fija que cw_tune_set_freq() y
 * cw_tune_ui_visible() pueden llamarse desde el hilo de audio sin tocar LVGL.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "cw_tune_stubs.h"

#include "styles.h"
#include "cfg/cfg.h"
#include "scheduler.h"

#include <stdlib.h>
#include <string.h>

/* --- puntuacion de hilos --------------------------------------------------- */

static pthread_t    lvgl_tid;
static bool         lvgl_tid_set;

static unsigned     lvgl_calls;
static unsigned     lvgl_foreign_calls;
static unsigned     refresh_n;
static unsigned     refresh_foreign;

static void note_lvgl(void) {
    lvgl_calls++;
    if (lvgl_tid_set && !pthread_equal(pthread_self(), lvgl_tid)) {
        lvgl_foreign_calls++;
    }
}

void cw_tune_stub_lvgl_thread(pthread_t tid) {
    lvgl_tid     = tid;
    lvgl_tid_set = true;
}

unsigned cw_tune_stub_lvgl_calls(void)          { return lvgl_calls; }
unsigned cw_tune_stub_lvgl_foreign_calls(void)  { return lvgl_foreign_calls; }
unsigned cw_tune_stub_refresh_events(void)      { return refresh_n; }
unsigned cw_tune_stub_refresh_foreign(void)     { return refresh_foreign; }

/* --- subjects (la implementacion real vive en src/cfg/subjects.cpp) --------- */

struct Subject {
    int32_t value;
};

struct ObserverDelayed {
    Subject              *subj;
    cw_tune_observer_fn_t fn;
    void                 *user_data;
};

static Subject         *mode_subject;
static ObserverDelayed *last_delayed;

int32_t subject_get_int(Subject *subj) {
    return subj->value;
}

ObserverDelayed *subject_add_delayed_observer(Subject *subj, cw_tune_observer_fn_t fn, void *user_data) {
    ObserverDelayed *o = calloc(1, sizeof(*o));

    o->subj      = subj;
    o->fn        = fn;
    o->user_data = user_data;
    last_delayed = o;      /* global: LSan lo ve alcanzable */

    return o;
}

cfg_cur_t cfg_cur;

/* El test arma el sujeto del modo una sola vez. */
void cw_tune_stub_reset(void) {
    lvgl_calls         = 0;
    lvgl_foreign_calls = 0;
    refresh_n          = 0;
    refresh_foreign    = 0;

    if (mode_subject == NULL) {
        mode_subject = calloc(1, sizeof(*mode_subject));
        cfg_cur.mode = mode_subject;
    }
}

void cw_tune_stub_mode(int32_t mode) {
    mode_subject->value = mode;
}

/* Corre los observers diferidos, como observer_delayed_notify_all() en el lazo
 * de GUI (src/main.c). */
void cw_tune_stub_delayed_notify(void) {
    if (last_delayed != NULL) {
        last_delayed->fn(last_delayed->subj, last_delayed->user_data);
    }
}

/* --- LVGL ----------------------------------------------------------------- */

lv_style_t cw_tune_style;

lv_obj_t *lv_obj_create(lv_obj_t *parent) {
    note_lvgl();

    lv_obj_t *o = calloc(1, sizeof(*o));

    o->parent = parent;

    return o;
}

void lv_obj_set_height(lv_obj_t *obj, lv_coord_t h) { note_lvgl(); (void) obj; (void) h; }
void lv_obj_set_width(lv_obj_t *obj, lv_coord_t w)  { note_lvgl(); (void) obj; (void) w; }

void lv_obj_add_flag(lv_obj_t *obj, uint32_t flag)   { note_lvgl(); obj->flags |= flag; }
void lv_obj_clear_flag(lv_obj_t *obj, uint32_t flag) { note_lvgl(); obj->flags &= ~flag; }
bool lv_obj_has_flag(const lv_obj_t *obj, uint32_t flag) { note_lvgl(); return (obj->flags & flag) != 0; }

void lv_obj_add_style(lv_obj_t *obj, const lv_style_t *style, uint32_t part) { note_lvgl(); (void) obj; (void) style; (void) part; }
void lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t code, void *user_data) {
    note_lvgl();
    (void) obj; (void) cb; (void) code; (void) user_data;
}
void lv_obj_get_coords(const lv_obj_t *obj, lv_area_t *coords) { note_lvgl(); (void) obj; memset(coords, 0, sizeof(*coords)); }
lv_obj_t *lv_obj_get_parent(const lv_obj_t *obj) { note_lvgl(); return obj->parent; }

lv_color_t lv_obj_get_style_bg_color(const lv_obj_t *obj, uint32_t part) {
    note_lvgl();
    (void) obj; (void) part;
    return (lv_color_t) { .full = 0 };
}

lv_obj_t      *lv_event_get_target(lv_event_t *e)    { note_lvgl(); return e->target; }
lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t *e)  { note_lvgl(); return e->draw_ctx; }

void lv_event_send(lv_obj_t *obj, lv_event_code_t code, void *param) {
    note_lvgl();

    if (code == LV_EVENT_REFRESH) {
        refresh_n++;
        if (lvgl_tid_set && !pthread_equal(pthread_self(), lvgl_tid)) {
            refresh_foreign++;
        }
    }

    (void) obj; (void) param;
}

void lv_area_move(lv_area_t *area, lv_coord_t dx, lv_coord_t dy) {
    note_lvgl();
    area->x1 += dx; area->y1 += dy; area->x2 += dx; area->y2 += dy;
}

void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *dsc) { note_lvgl(); memset(dsc, 0, sizeof(*dsc)); }
void lv_draw_rect(lv_draw_ctx_t *ctx, const lv_draw_rect_dsc_t *dsc, const lv_area_t *area) {
    note_lvgl();
    (void) ctx; (void) dsc; (void) area;
}

lv_color_t lv_color_hex(uint32_t c)          { return (lv_color_t) { .full = c }; }
lv_color_t lv_color_white(void)              { return (lv_color_t) { .full = 0xFFFFFF }; }
lv_color_t lv_color_black(void)              { return (lv_color_t) { .full = 0x000000 }; }
lv_color_t lv_color_make(uint8_t r, uint8_t g, uint8_t b) {
    return (lv_color_t) { .full = ((uint32_t) r << 16) | ((uint32_t) g << 8) | b };
}
uint32_t lv_color_to32(lv_color_t color) {
    return color.full;
}
/* --- scheduler falso ------------------------------------------------------- */

#define SCHED_MAX 64

typedef struct {
    scheduler_fn_t fn;
    void          *arg;
} sched_item_t;

static sched_item_t sched_q[SCHED_MAX];
static unsigned     sched_read;
static unsigned     sched_write;

/* Hilo de GUI del test: lo fija el runner con scheduler_thread_init(). */
static pthread_t    gui_tid;
static bool         gui_tid_set;

void scheduler_thread_init(void) {
    gui_tid     = pthread_self();
    gui_tid_set = true;
}

bool scheduler_in_main_thread(void) {
    return !gui_tid_set || pthread_equal(pthread_self(), gui_tid);
}

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

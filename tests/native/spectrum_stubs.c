/*
 * Stubs for the native spectrum test (issue #149).
 *
 * The real src/spectrum.c runs here against a minimal LVGL and against a
 * test-owned Subject/cfg/params surface. What the stub adds on top of "make it
 * link" is instrumentation:
 *
 *   - every lv_draw_line / lv_draw_rect / lv_obj_invalidate is recorded with
 *     its geometry and the calling thread, so the test can rebuild what the
 *     callback painted and from which frame generation;
 *   - the draw can be stopped at an exact line (spectrum_stub_pause_at) so the
 *     producer is interleaved *deterministically*, not by luck;
 *   - the fake scheduler counts what is queued and its peak, and can reject a
 *     put on demand (the queue-full path).
 *
 * Every entry point records the calling thread: LVGL may only be touched from
 * the GUI thread (issue #149, experiment 4), and the test asserts it.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "spectrum_stubs.h"

#include "lvgl/lvgl.h"
#include "scheduler.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "meter.h"
#include "params/params.h"
#include "radio.h"
#include "recorder.h"
#include "rtty.h"
#include "theme.h"
#include "util.h"

/* --- estado global del test ------------------------------------------------ */

params_t params;

static pthread_mutex_t stub_mux = PTHREAD_MUTEX_INITIALIZER;

spectrum_stub_line_t spectrum_stub_lines[SPECTRUM_STUB_MAX_LINES];
unsigned             spectrum_stub_lines_n;
spectrum_stub_rect_t spectrum_stub_rects[SPECTRUM_STUB_MAX_RECTS];
unsigned             spectrum_stub_rects_n;
unsigned             spectrum_stub_foreign_lvgl;
unsigned             spectrum_stub_invalidates;

static pthread_t gui_tid;
static bool      gui_tid_set;

lv_style_t spectrum_style;
lv_color_t bg_color = { .full = 0x101010u };

static void note_lvgl(void) {
    pthread_mutex_lock(&stub_mux);

    if (gui_tid_set && !pthread_equal(pthread_self(), gui_tid)) {
        spectrum_stub_foreign_lvgl++;
    }

    pthread_mutex_unlock(&stub_mux);
}

/* --- LVGL ----------------------------------------------------------------- */

#define STUB_WIDGET_W SPECTRUM_STUB_W
#define STUB_WIDGET_H SPECTRUM_STUB_H

static lv_obj_t      widget;
static lv_draw_ctx_t draw_ctx;

static lv_event_cb_t draw_cb;

uint32_t EVENT_RADIO_TX = 1000;
uint32_t EVENT_RADIO_RX = 1001;

lv_obj_t *lv_obj_create(lv_obj_t *parent) {
    (void) parent;

    widget.coords.x1 = 0;
    widget.coords.y1 = 0;
    widget.coords.x2 = STUB_WIDGET_W - 1;
    widget.coords.y2 = STUB_WIDGET_H - 1;
    widget.w = STUB_WIDGET_W;
    widget.h = STUB_WIDGET_H;
    return &widget;
}

void lv_obj_add_style(lv_obj_t *obj, const lv_style_t *style, uint32_t part) {
    (void) obj; (void) style; (void) part;
}

void lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t code, void *user_data) {
    (void) obj; (void) user_data;

    if (code == LV_EVENT_DRAW_MAIN_END) {
        draw_cb = cb;
    }
}

lv_obj_t *lv_event_get_target(lv_event_t *e) {
    return e->target;
}

lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t *e) {
    return e->draw_ctx;
}

lv_coord_t lv_obj_get_width(const lv_obj_t *obj) {
    return obj->w;
}

lv_coord_t lv_obj_get_height(const lv_obj_t *obj) {
    return obj->h;
}

void lv_draw_line_dsc_init(lv_draw_line_dsc_t *dsc) {
    memset(dsc, 0, sizeof(*dsc));
}

void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *dsc) {
    memset(dsc, 0, sizeof(*dsc));
}

lv_color_t lv_color_hex(uint32_t c) {
    lv_color_t color = { .full = c };
    return color;
}

lv_color_t lv_color_white(void) {
    lv_color_t color = { .full = 0xFFFFFFu };
    return color;
}

void lv_obj_invalidate(lv_obj_t *obj) {
    (void) obj;

    note_lvgl();

    pthread_mutex_lock(&stub_mux);
    spectrum_stub_invalidates++;
    pthread_mutex_unlock(&stub_mux);
}

/* --- protocolo de pausa ---------------------------------------------------- */

#define PAUSE_TIMEOUT_MS 2000u

static pthread_mutex_t pause_mux = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  paused_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  resume_cv = PTHREAD_COND_INITIALIZER;

static int  pause_index = -1;
static bool paused;
static bool resume_requested;
static bool pause_timeout;

static struct timespec deadline_ms(unsigned ms) {
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000u;
    ts.tv_nsec += (long) (ms % 1000u) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return ts;
}

static void pause_at_line(unsigned index) {
    pthread_mutex_lock(&pause_mux);

    if (pause_index < 0 || pause_index != (int) index) {
        pthread_mutex_unlock(&pause_mux);
        return;
    }

    paused = true;
    pthread_cond_broadcast(&paused_cv);

    struct timespec ts = deadline_ms(PAUSE_TIMEOUT_MS);

    while (!resume_requested) {
        if (pthread_cond_timedwait(&resume_cv, &pause_mux, &ts) == ETIMEDOUT) {
            pause_timeout = true;
            break;
        }
    }

    resume_requested = false;
    paused = false;

    pthread_mutex_unlock(&pause_mux);
}

void spectrum_stub_pause_at(int index) {
    pthread_mutex_lock(&pause_mux);
    pause_index = index;
    pthread_mutex_unlock(&pause_mux);
}

bool spectrum_stub_wait_paused(unsigned timeout_ms) {
    bool ok = false;

    pthread_mutex_lock(&pause_mux);

    struct timespec ts = deadline_ms(timeout_ms);

    while (!paused && !pause_timeout) {
        if (pthread_cond_timedwait(&paused_cv, &pause_mux, &ts) == ETIMEDOUT) {
            break;
        }
    }

    ok = paused;

    pthread_mutex_unlock(&pause_mux);

    return ok;
}

void spectrum_stub_resume(void) {
    pthread_mutex_lock(&pause_mux);
    resume_requested = true;
    pthread_cond_broadcast(&resume_cv);
    pthread_mutex_unlock(&pause_mux);
}

bool spectrum_stub_pause_timed_out(void) {
    bool v;

    pthread_mutex_lock(&pause_mux);
    v = pause_timeout;
    pthread_mutex_unlock(&pause_mux);

    return v;
}

/* --- primitivas de dibujo -------------------------------------------------- */

void lv_draw_line(lv_draw_ctx_t *ctx, const lv_draw_line_dsc_t *dsc,
                  const lv_point_t *p1, const lv_point_t *p2) {
    (void) ctx;

    note_lvgl();

    unsigned index;

    pthread_mutex_lock(&stub_mux);

    index = spectrum_stub_lines_n;

    if (index < SPECTRUM_STUB_MAX_LINES) {
        spectrum_stub_line_t *line = &spectrum_stub_lines[index];

        line->color = dsc->color.full;
        line->x1    = p1->x;
        line->y1    = p1->y;
        line->x2    = p2->x;
        line->y2    = p2->y;
    }

    spectrum_stub_lines_n++;

    pthread_mutex_unlock(&stub_mux);

    pause_at_line(index);
}

void lv_draw_rect(lv_draw_ctx_t *ctx, const lv_draw_rect_dsc_t *dsc, const lv_area_t *area) {
    (void) ctx;

    note_lvgl();

    pthread_mutex_lock(&stub_mux);

    if (spectrum_stub_rects_n < SPECTRUM_STUB_MAX_RECTS) {
        spectrum_stub_rect_t *rect = &spectrum_stub_rects[spectrum_stub_rects_n];

        rect->bg = dsc->bg_color.full;
        rect->x1 = area->x1;
        rect->y1 = area->y1;
        rect->x2 = area->x2;
        rect->y2 = area->y2;
    }

    spectrum_stub_rects_n++;

    pthread_mutex_unlock(&stub_mux);
}

/* --- scheduler falso ------------------------------------------------------- */

#define SCHED_MAX 256

static scheduler_fn_t sched_fn[SCHED_MAX];
static unsigned       sched_n;
static unsigned       sched_peak;
static unsigned       sched_reject;

bool scheduler_put(scheduler_fn_t fn, void *arg, size_t arg_size) {
    (void) arg; (void) arg_size;

    if (fn == NULL) {
        return false;
    }

    pthread_mutex_lock(&stub_mux);

    if (sched_reject > 0) {
        sched_reject--;
        pthread_mutex_unlock(&stub_mux);
        return false;
    }

    if (sched_n >= SCHED_MAX) {
        pthread_mutex_unlock(&stub_mux);
        return false;
    }

    sched_fn[sched_n++] = fn;
    if (sched_n > sched_peak) {
        sched_peak = sched_n;
    }

    pthread_mutex_unlock(&stub_mux);

    return true;
}

bool scheduler_put_noargs(scheduler_fn_t fn) {
    return scheduler_put(fn, NULL, 0);
}

void scheduler_work(void) {
    spectrum_stub_sched_drain();
}

unsigned spectrum_stub_sched_pending(void) {
    unsigned n;

    pthread_mutex_lock(&stub_mux);
    n = sched_n;
    pthread_mutex_unlock(&stub_mux);

    return n;
}

unsigned spectrum_stub_sched_peak(void) {
    unsigned n;

    pthread_mutex_lock(&stub_mux);
    n = sched_peak;
    pthread_mutex_unlock(&stub_mux);

    return n;
}

void spectrum_stub_sched_reject_next(unsigned times) {
    pthread_mutex_lock(&stub_mux);
    sched_reject = times;
    pthread_mutex_unlock(&stub_mux);
}

void spectrum_stub_sched_drain(void) {
    for (;;) {
        scheduler_fn_t fn;
        bool           have = false;

        pthread_mutex_lock(&stub_mux);
        if (sched_n > 0) {
            fn = sched_fn[0];
            memmove(&sched_fn[0], &sched_fn[1], (sched_n - 1) * sizeof(sched_fn[0]));
            sched_n--;
            have = true;
        }
        pthread_mutex_unlock(&stub_mux);

        if (!have) {
            return;
        }

        fn(NULL);
    }
}

/* --- subjects, cfg y params ------------------------------------------------ */

#define MAX_OBSERVERS 4

struct Subject {
    int32_t                     value;
    unsigned                    observers_n;
    spectrum_observer_fn_t      observers[MAX_OBSERVERS];
    void                       *user_data[MAX_OBSERVERS];
};

static Subject subj_fg_freq;
static Subject subj_mode;
static Subject subj_fft_width;
static Subject subj_filter_from;
static Subject subj_filter_to;
static Subject subj_grid_min;
static Subject subj_grid_max;
static Subject subj_dnf;
static Subject subj_dnf_center;
static Subject subj_dnf_width;

static cfg_band_t band;

cfg_cur_t cfg_cur;
cfg_t     cfg;

int32_t subject_get_int(Subject *subj) {
    int32_t v;

    pthread_mutex_lock(&stub_mux);
    v = subj->value;
    pthread_mutex_unlock(&stub_mux);

    return v;
}

Subject *subject_add_observer_and_call(Subject *subj, spectrum_observer_fn_t fn, void *user_data) {
    pthread_mutex_lock(&stub_mux);
    if (subj->observers_n < MAX_OBSERVERS) {
        subj->observers[subj->observers_n] = fn;
        subj->user_data[subj->observers_n] = user_data;
        subj->observers_n++;
    }
    pthread_mutex_unlock(&stub_mux);

    /* Fuera del lock: el observer puede tomar el mutex del modulo. */
    fn(subj, user_data);

    return subj;
}

void spectrum_stub_subject_set(Subject *subj, int32_t value) {
    spectrum_observer_fn_t cbs[MAX_OBSERVERS];
    void                  *ud[MAX_OBSERVERS];
    unsigned               n;
    bool                   changed;

    pthread_mutex_lock(&stub_mux);

    changed = (subj->value != value);
    if (changed) {
        subj->value = value;
    }

    n = subj->observers_n;
    memcpy(cbs, subj->observers, sizeof(cbs));
    memcpy(ud, subj->user_data, sizeof(ud));

    pthread_mutex_unlock(&stub_mux);

    if (!changed) {
        return;
    }

    for (unsigned i = 0; i < n; i++) {
        cbs[i](subj, ud[i]);
    }
}

/* El reloj no avanza: el decaimiento del pico (now - time > hold) queda en
 * falso, asi el pico de una generacion no se mezcla con el de la siguiente por
 * una deriva de tiempo. */
uint64_t get_time(void) {
    return 1000000u;
}

/* Misma ley que src/util.cpp (el test no depende del valor exacto, pero una
 * divergencia de la forma del filtro tiene que aparecer aca y no en el test). */
void lpf(float *x, float current, float beta, float initial) {
    if (*x == initial) {
        *x = current;
    } else {
        *x = *x * beta + current * (1.0f - beta);
    }
}

uint32_t theme_hex(theme_color_t c) {
    switch (c) {
        case TC_text_dim:  return SPECTRUM_STUB_COLOR_MAIN;
        case TC_text_mute: return SPECTRUM_STUB_COLOR_PEAK;
        case TC_tx:        return SPECTRUM_STUB_COLOR_TX;
    }

    return 0;
}

bool recorder_is_on(void) {
    return false;
}

rtty_state_t rtty_get_state(void) {
    return RTTY_OFF;
}

/* --- arranque del test ----------------------------------------------------- */

static bool subjects_ready;

static void subjects_build(void) {
    if (subjects_ready) {
        return;
    }

    subj_fft_width.value   = 100000;
    subj_filter_from.value = 0;
    subj_filter_to.value   = 3000;
    subj_mode.value        = x6200_mode_usb;
    subj_fg_freq.value     = 14074000;
    subj_grid_min.value    = S4;
    subj_grid_max.value    = S9_20;
    subj_dnf.value         = x6200_dnf_off;

    band.grid.min.val = &subj_grid_min;
    band.grid.max.val = &subj_grid_max;

    cfg_cur.band       = &band;
    cfg_cur.fg_freq    = &subj_fg_freq;
    cfg_cur.mode       = &subj_mode;
    cfg_cur.fft_width  = &subj_fft_width;
    cfg_cur.filter.real.from = &subj_filter_from;
    cfg_cur.filter.real.to   = &subj_filter_to;

    cfg.dnf.val        = &subj_dnf;
    cfg.dnf_center.val = &subj_dnf_center;
    cfg.dnf_width.val  = &subj_dnf_width;

    subjects_ready = true;
}

void spectrum_stub_gui_thread(void) {
    pthread_mutex_lock(&stub_mux);
    gui_tid = pthread_self();
    gui_tid_set = true;
    pthread_mutex_unlock(&stub_mux);
}

void spectrum_stub_reset(void) {
    subjects_build();

    /* Drena lo que haya quedado encolado ANTES de limpiar los contadores: el
     * refresh encolado es lo que libera el ticket del modulo, y descartarlo sin
     * correrlo dejaria al productor sin encolar nunca mas. */
    spectrum_stub_sched_drain();

    pthread_mutex_lock(&stub_mux);
    spectrum_stub_lines_n = 0;
    spectrum_stub_rects_n = 0;
    spectrum_stub_foreign_lvgl = 0;
    spectrum_stub_invalidates = 0;
    sched_n = 0;
    sched_peak = 0;
    sched_reject = 0;
    pthread_mutex_unlock(&stub_mux);

    pthread_mutex_lock(&pause_mux);
    pause_index = -1;
    paused = false;
    resume_requested = false;
    pause_timeout = false;
    pthread_mutex_unlock(&pause_mux);

    params.spectrum_peak       = false;
    params.spectrum_peak_hold  = 1000;
    params.spectrum_peak_speed = 5.0f;
    params.spectrum_filled     = false;
    params.spectrum_auto_min.x = 0.0f;
    params.spectrum_auto_max.x = 0.0f;
    params.rtty_center         = 700;
    params.rtty_shift          = 170;
}

void spectrum_stub_clear_records(void) {
    pthread_mutex_lock(&stub_mux);
    spectrum_stub_lines_n = 0;
    spectrum_stub_rects_n = 0;
    pthread_mutex_unlock(&stub_mux);
}

void spectrum_stub_draw(void) {
    lv_event_t e = { .target = &widget, .draw_ctx = &draw_ctx };

    if (draw_cb == NULL) {
        return;
    }

    draw_cb(&e);
}

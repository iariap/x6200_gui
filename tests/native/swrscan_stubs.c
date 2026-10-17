/*
 * Stubs for the native SWR Scan dialog test (tests/native/test_swrscan.c,
 * issues #85 and #119). The REAL src/dialog_swrscan.c is compiled against these
 * (staged next to them, the same pattern as the clock/backlight/digital-modes
 * tests); only its collaborators are faked here.
 *
 * Since #119 the test also drives the dialog's draw callback and reads the line
 * segments it paints (swrscan_stub_draw() + the swrscan_stub_curve_*() accessors):
 * the shape of the drawn curve is an assertion of its own (a smooth parabola, not
 * the staircase of the Catmull-Rom resampler the flashed ROM ran).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "params/params.h"
#include "radio.h"
#include "buttons.h"
#include "dialog.h"
#include "main_screen.h"
#include "events.h"
#include "util.h"
#include "keyboard.h"
#include "theme.h"
#include "scheduler.h"
#include "lvgl/lvgl.h"

/* --- recorded calls, read by the test -------------------------------------- */

static int stub_null_label;
static int stub_started;
static int stub_stopped;
static int stub_destructed;

/* Reloj virtual (issue #125): el motor mide el timeout de RF con get_time(); el test lo mueve
 * a mano, sin sleeps reales. */
static uint64_t stub_time;

void     swrscan_stub_time_set(uint64_t t) { stub_time = t; }
void     swrscan_stub_time_advance(uint64_t d) { stub_time += d; }
uint64_t get_time(void) { return stub_time; }

/* El cierre del lado de LVGL lo encola el hilo de la radio por el scheduler. El stub lo guarda
 * para que el test lo corra cuando quiere (como haria el hilo principal). */
static scheduler_fn_t stub_pending_job;
static int            stub_pending_jobs;

void scheduler_put_noargs(scheduler_fn_t fn) {
    stub_pending_job = fn;
    stub_pending_jobs++;
}

void swrscan_stub_run_scheduler(void) {
    scheduler_fn_t fn = stub_pending_job;

    stub_pending_job = NULL;

    if (fn) {
        fn(NULL);
    }
}

int swrscan_stub_pending_jobs(void) { return stub_pending_jobs; }

/* The sweep moves the VFO one point per received packet (do_step -> radio_set_freq): the
 * test reads this sequence to pin the sweep geometry (issue #113). Since #125 the sequence
 * covers a whole session: 51 points x 5 passes + the return to the app frequency = 256. */
#define STUB_FREQS 256

static int32_t stub_freqs[STUB_FREQS];
static int     stub_freq_count;

int swrscan_stub_null_label(void) { return stub_null_label; }
int swrscan_stub_started(void)    { return stub_started; }
int swrscan_stub_stopped(void)    { return stub_stopped; }
int swrscan_stub_destructed(void) { return stub_destructed; }
int32_t swrscan_stub_freq(int i)      { return stub_freqs[i]; }
int     swrscan_stub_freq_count(void) { return stub_freq_count; }

void swrscan_stub_reset(void) {
    stub_null_label = 0;
    stub_started    = 0;
    stub_stopped    = 0;
    stub_destructed = 0;
    stub_freq_count = 0;
    stub_pending_job  = NULL;
    stub_pending_jobs = 0;
    stub_time         = 0;
}

/* --- theme ------------------------------------------------------------------ */

/* The test build stubs the theme (tests/native/stubs_swrscan/theme.h): each role resolves to a
 * fixed colour, because the assertions are that the chart paints its own background and that its
 * text comes from the theme's role — not which colour the theme gives it (that is the theme
 * tests' job). */
uint32_t theme_hex(theme_color_t c) {
    return (c == TC_text) ? 0x445566 : 0x112233;
}

/* --- draw recording (issue #119) -------------------------------------------- */

/* The test drives the dialog's draw callback (swrscan_stub_draw) and reads the line segments
 * it painted. The chart's curve is the only line with width 4 (the grid and the axis lines use
 * width 2), so the accessors below return that subset: the drawn polyline. */
#define STUB_SEGMENTS 1024

typedef struct {
    lv_point_t a;
    lv_point_t b;
    lv_coord_t width;
    lv_color_t color;
} stub_segment_t;

static stub_segment_t stub_segments[STUB_SEGMENTS];
static int            stub_segment_count;

static lv_obj_t      *draw_obj;         /* the object the DRAW_MAIN_END callback was registered on */
static lv_event_cb_t  draw_cb;
static lv_obj_t      *event_target;     /* what lv_event_get_target() hands the callback */
static lv_color_t     last_label_color; /* the colour of the last text the chart painted */
static char           last_label_text[160]; /* the last text the chart painted (#125 readout) */

/* El fondo del bloque estado+readout (issue #125): el texto se pinta encima de la grilla, de
 * las etiquetas del eje y de la curva, asi que va sobre un rect del color del panel del chart
 * con opacidad parcial. El test lee el ultimo rect pintado para fijar esa legibilidad. */
static int        stub_rect_count;
static lv_color_t stub_rect_color;
static uint8_t    stub_rect_opa;

/* Fires the dialog's draw callback on the chart, recording what it paints. */
void swrscan_stub_draw(void) {
    stub_segment_count = 0;
    stub_rect_count    = 0;
    event_target       = draw_obj;
    last_label_color   = lv_color_hex(0);

    if (draw_cb) {
        draw_cb(NULL);
    }

    event_target = NULL;
}

/* Number of painted segments (all of them, grid included). */
int swrscan_stub_segment_count(void) { return stub_segment_count; }

/* The curve: the segments the dialog paints with width 4. `end` = 1 takes the segment's end
 * point, 0 its start, so segment i of the curve joins (x(i), y(i)) to (x(i+1), y(i+1)). */
int swrscan_stub_curve_segment_count(void) {
    int n = 0;

    for (int i = 0; i < stub_segment_count; i++) {
        if (stub_segments[i].width == 4) {
            n++;
        }
    }

    return n;
}

static const stub_segment_t *curve_segment(int i) {
    int n = 0;

    for (int k = 0; k < stub_segment_count; k++) {
        if (stub_segments[k].width == 4) {
            if (n == i) {
                return &stub_segments[k];
            }
            n++;
        }
    }

    return NULL;
}

lv_coord_t swrscan_stub_curve_x(int i, int end) {
    const stub_segment_t *s = curve_segment(i);

    if (!s) {
        return 0;
    }

    return end ? s->b.x : s->a.x;
}

lv_coord_t swrscan_stub_curve_y(int i, int end) {
    const stub_segment_t *s = curve_segment(i);

    if (!s) {
        return 0;
    }

    return end ? s->b.y : s->a.y;
}

/* El color con el que el chart dibuja la curva y el texto: sale del rol `text` del tema, que es
 * lo que lo hace legible sobre el panel del chart (claro u oscuro segun el tema). */
lv_color_t swrscan_stub_curve_color(void) {
    const stub_segment_t *s = curve_segment(0);

    return s ? s->color : last_label_color;
}

lv_color_t swrscan_stub_label_color(void) {
    return last_label_color;
}

/* El fondo del bloque estado+readout (issue #125): cuantos rects pinto el chart en el ultimo
 * draw, y con que color/opacidad. El texto va encima de la grilla y de la curva, asi que el
 * test fija que se pinte UN rect del color del panel del chart con opacidad parcial. */
int swrscan_stub_rect_count(void) {
    return stub_rect_count;
}

lv_color_t swrscan_stub_rect_color(void) {
    return stub_rect_color;
}

uint8_t swrscan_stub_rect_opa(void) {
    return stub_rect_opa;
}

/* The chart's own background: what construct_cb set on the object it draws into. The test
 * pins that the chart paints an opaque panel of its own (issue #119, the ghost of the main
 * screen showing through the chart) — 0 means the object has no background at all. */
uint8_t swrscan_stub_chart_bg_opa(void) {
    return draw_obj ? draw_obj->bg_opa : 0;
}

int swrscan_stub_chart_bg_color_set(void) {
    return draw_obj && draw_obj->bg_color_set;
}

/* --- cfg / cfg_cur ---------------------------------------------------------- */

struct Subject {
    int32_t ival;
};

static struct Subject subj_fg_freq;
static struct Subject subj_mode;
static struct Subject subj_swrscan_linear;
static struct Subject subj_swrscan_span;
static struct Subject subj_pwr;

cfg_t     cfg = {
    .swrscan_linear = {.val = &subj_swrscan_linear},
    .swrscan_span   = {.val = &subj_swrscan_span},
    .pwr            = {.val = &subj_pwr},
};
cfg_cur_t cfg_cur = {
    .fg_freq = &subj_fg_freq,
    .mode    = &subj_mode,
};

void swrscan_stub_set_fg_freq(int32_t v) { subj_fg_freq.ival = v; }
void swrscan_stub_set_span(int32_t v)    { subj_swrscan_span.ival = v; }

int32_t subject_get_int(Subject *subj) { return subj->ival; }
void    subject_set_int(Subject *subj, int32_t val) { subj->ival = val; }

struct ObserverDelayed {
    int dummy;
};

static struct ObserverDelayed observer;

ObserverDelayed *subject_add_delayed_observer(Subject *subj, void (*fn)(Subject *, void *), void *user_data) {
    (void)subj;
    (void)fn;
    (void)user_data;
    return &observer;
}

ObserverDelayed *subject_add_delayed_observer_and_call(Subject *subj, void (*fn)(Subject *, void *), void *user_data) {
    fn(subj, user_data);
    return &observer;
}

void observer_delayed_del(ObserverDelayed *observer) { (void)observer; }

/* --- buttons ---------------------------------------------------------------- */

void button_update_label(button_item_t *item, char *label) {
    if (item == NULL) {
        /* The real src/buttons.cpp dereferences item->type here: NULL + 4 is the
         * fallo=0x4 of issue #85. The test records it instead of crashing. */
        stub_null_label++;
        return;
    }
    if (item->type == BTN_TEXT) {
        item->label = label;
    }
}

void buttons_unload_page(void) {}
void buttons_load_page(buttons_page_t *page) { (void)page; }

/* --- dialog ----------------------------------------------------------------- */

static lv_obj_t dialog_obj;

lv_obj_t *dialog_init(lv_obj_t *parent) {
    (void)parent;
    return &dialog_obj;
}

void dialog_construct(dialog_t *dialog, lv_obj_t *parent) { (void)dialog; (void)parent; }

/* The real dialog_destruct() deletes the dialog's object: the app closes. The stub records it
 * instead (issue #119: el fin del barrido no puede cerrar la app). */
void dialog_destruct(void) { stub_destructed++; }

/* --- radio ------------------------------------------------------------------ */

void radio_set_freq(int32_t freq) {
    if (stub_freq_count < STUB_FREQS) {
        stub_freqs[stub_freq_count++] = freq;
    }
}
uint16_t radio_change_vol(int16_t df) { (void)df; return 0; }

bool radio_start_swrscan(void) {
    stub_started++;
    return true;
}

void radio_stop_swrscan(void) {
    stub_stopped++;
}

/* --- misc ------------------------------------------------------------------- */

struct _lv_font_t {
    int dummy;
};

lv_font_t sony_28;

struct _lv_group_t {
    int dummy;
};

static struct _lv_group_t group;
lv_group_t *keyboard_group = &group;

void event_send(lv_obj_t *obj, lv_event_code_t event_code, void *param) {
    (void)obj;
    (void)event_code;
    (void)param;
}

void split_freq(int32_t freq, uint16_t *mhz, uint16_t *khz, uint16_t *hz) {
    *mhz = (uint16_t)(freq / 1000000);
    *khz = (uint16_t)((freq / 1000) % 1000);
    *hz  = (uint16_t)(freq % 1000);
}

void mem_load(uint16_t id) { (void)id; }
void mem_save(uint16_t id) { (void)id; }

/* --- lvgl ------------------------------------------------------------------- */

static lv_obj_t obj_pool[4];
static int      obj_next;
static lv_event_cb_t delete_cb;

lv_obj_t *lv_obj_create(lv_obj_t *parent) {
    (void)parent;
    return &obj_pool[obj_next++ % 4];
}

void lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t filter, void *user_data) {
    (void)user_data;
    if (filter == LV_EVENT_DELETE) {
        delete_cb = cb;
    } else if (filter == LV_EVENT_DRAW_MAIN_END) {
        /* The chart: the test fires this callback through swrscan_stub_draw(). */
        draw_obj = obj;
        draw_cb  = cb;
    }
}

void swrscan_stub_fire_delete(void) {
    if (delete_cb) {
        delete_cb(NULL);
    }
}

int swrscan_stub_delete_cb_registered(void) { return delete_cb != NULL; }

void lv_obj_set_size(lv_obj_t *obj, lv_coord_t w, lv_coord_t h) {
    obj->w = w;
    obj->h = h;
}

/* The chart is the only object the dialog centers. Its real place on the 800x480 panel
 * (measured on the radio's frames: the chart is 780x330 centered in the dialog box that
 * starts at y = 66) so the drawn geometry matches what the panel shows. */
void lv_obj_center(lv_obj_t *obj) {
    obj->coords.x1 = 10;
    obj->coords.y1 = 74;
    obj->coords.x2 = 790;
    obj->coords.y2 = 404;
}

void lv_obj_set_style_bg_opa(lv_obj_t *obj, uint8_t opa, uint32_t part) {
    (void)part;
    obj->bg_opa = opa;
}

void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t color, uint32_t part) {
    (void)color;
    (void)part;
    obj->bg_color_set = true;
}

void lv_obj_set_style_border_width(lv_obj_t *obj, lv_coord_t width, uint32_t part) { (void)obj; (void)width; (void)part; }
void lv_group_add_obj(lv_group_t *group, lv_obj_t *obj) { (void)group; (void)obj; }
lv_coord_t lv_obj_get_width(lv_obj_t *obj) { return obj->w; }
lv_coord_t lv_obj_get_height(lv_obj_t *obj) { return obj->h; }
void lv_obj_invalidate(lv_obj_t *obj) { (void)obj; }

lv_obj_t      *lv_event_get_target(lv_event_t *e) { (void)e; return event_target; }
lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t *e) { (void)e; return NULL; }
void          *lv_event_get_param(lv_event_t *e) { (void)e; return NULL; }

void lv_draw_line_dsc_init(lv_draw_line_dsc_t *dsc) { memset(dsc, 0, sizeof(*dsc)); }
void lv_draw_label_dsc_init(lv_draw_label_dsc_t *dsc) { memset(dsc, 0, sizeof(*dsc)); }
lv_color_t lv_color_white(void) { lv_color_t c = {255, 255, 255}; return c; }
lv_color_t lv_color_hex(uint32_t v) {
    /* Decoded, so the test can tell the theme's role apart from a colour written in the code.
     * The struct's fields are blue, green, red (like LVGL's lv_color_t). */
    lv_color_t c = {(uint8_t) (v & 0xff), (uint8_t) ((v >> 8) & 0xff), (uint8_t) ((v >> 16) & 0xff)};
    return c;
}
void lv_draw_line(lv_draw_ctx_t *ctx, const lv_draw_line_dsc_t *dsc, const lv_point_t *p1, const lv_point_t *p2) {
    (void)ctx;

    if (stub_segment_count < STUB_SEGMENTS) {
        stub_segments[stub_segment_count].a     = *p1;
        stub_segments[stub_segment_count].b     = *p2;
        stub_segments[stub_segment_count].width = dsc->width;
        stub_segments[stub_segment_count].color = dsc->color;
        stub_segment_count++;
    }
}
void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *dsc) { memset(dsc, 0, sizeof(*dsc)); }
void lv_draw_rect(lv_draw_ctx_t *ctx, const lv_draw_rect_dsc_t *dsc, const lv_area_t *coords) {
    (void)ctx; (void)coords;

    stub_rect_count++;
    stub_rect_color = dsc->bg_color;
    stub_rect_opa   = dsc->bg_opa;
}
void lv_draw_label(lv_draw_ctx_t *ctx, const lv_draw_label_dsc_t *dsc, const lv_area_t *coords, const char *txt, void *bidi) {
    (void)ctx; (void)coords; (void)bidi;
    last_label_color = dsc->color;

    /* El test lee el ultimo texto dibujado (el readout de estado/minimo del #125). */
    snprintf(last_label_text, sizeof(last_label_text), "%s", txt ? txt : "");
}

const char *swrscan_stub_last_label_text(void) {
    return last_label_text;
}
void lv_txt_get_size(lv_point_t *size_res, const char *txt, const lv_font_t *font,
                     lv_coord_t letter_space, lv_coord_t line_space, lv_coord_t max_width, uint32_t flag) {
    (void)txt; (void)font; (void)letter_space; (void)line_space; (void)max_width; (void)flag;
    size_res->x = 0;
    size_res->y = 0;
}

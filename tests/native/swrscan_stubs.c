/*
 * Stubs for the native SWR Scan dialog test (tests/native/test_swrscan.c,
 * issue #85). The REAL src/dialog_swrscan.c is compiled against these (staged
 * next to them, the same pattern as the clock/backlight/digital-modes tests);
 * only its collaborators are faked here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>
#include <string.h>

#include "params/params.h"
#include "radio.h"
#include "buttons.h"
#include "dialog.h"
#include "main_screen.h"
#include "events.h"
#include "util.h"
#include "keyboard.h"
#include "lvgl/lvgl.h"

/* --- recorded calls, read by the test -------------------------------------- */

static int stub_null_label;
static int stub_started;
static int stub_stopped;

int swrscan_stub_null_label(void) { return stub_null_label; }
int swrscan_stub_started(void)    { return stub_started; }
int swrscan_stub_stopped(void)    { return stub_stopped; }

void swrscan_stub_reset(void) {
    stub_null_label = 0;
    stub_started    = 0;
    stub_stopped    = 0;
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
void dialog_destruct(void) {}

/* --- radio ------------------------------------------------------------------ */

static int32_t last_freq;

void radio_set_freq(int32_t freq) { last_freq = freq; }
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
    (void)obj;
    (void)user_data;
    if (filter == LV_EVENT_DELETE) {
        delete_cb = cb;
    }
}

void swrscan_stub_fire_delete(void) {
    if (delete_cb) {
        delete_cb(NULL);
    }
}

int swrscan_stub_delete_cb_registered(void) { return delete_cb != NULL; }

void      lv_obj_set_size(lv_obj_t *obj, lv_coord_t w, lv_coord_t h) { (void)obj; (void)w; (void)h; }
void      lv_obj_center(lv_obj_t *obj) { (void)obj; }
void      lv_obj_set_style_bg_opa(lv_obj_t *obj, uint8_t opa, uint32_t part) { (void)obj; (void)opa; (void)part; }
void      lv_obj_set_style_border_width(lv_obj_t *obj, lv_coord_t width, uint32_t part) { (void)obj; (void)width; (void)part; }
void      lv_group_add_obj(lv_group_t *group, lv_obj_t *obj) { (void)group; (void)obj; }
lv_coord_t lv_obj_get_width(lv_obj_t *obj) { (void)obj; return 0; }
lv_coord_t lv_obj_get_height(lv_obj_t *obj) { (void)obj; return 0; }
void      lv_obj_invalidate(lv_obj_t *obj) { (void)obj; }

lv_obj_t      *lv_event_get_target(lv_event_t *e) { (void)e; return NULL; }
lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t *e) { (void)e; return NULL; }
void          *lv_event_get_param(lv_event_t *e) { (void)e; return NULL; }

void lv_draw_line_dsc_init(lv_draw_line_dsc_t *dsc) { memset(dsc, 0, sizeof(*dsc)); }
void lv_draw_label_dsc_init(lv_draw_label_dsc_t *dsc) { memset(dsc, 0, sizeof(*dsc)); }
lv_color_t lv_color_white(void) { lv_color_t c = {0, 0, 0}; return c; }
lv_color_t lv_color_hex(uint32_t v) { (void)v; lv_color_t c = {0, 0, 0}; return c; }
void lv_draw_line(lv_draw_ctx_t *ctx, const lv_draw_line_dsc_t *dsc, const lv_point_t *p1, const lv_point_t *p2) {
    (void)ctx; (void)dsc; (void)p1; (void)p2;
}
void lv_draw_label(lv_draw_ctx_t *ctx, const lv_draw_label_dsc_t *dsc, const lv_area_t *coords, const char *txt, void *bidi) {
    (void)ctx; (void)dsc; (void)coords; (void)txt; (void)bidi;
}
void lv_txt_get_size(lv_point_t *size_res, const char *txt, const lv_font_t *font,
                     lv_coord_t letter_space, lv_coord_t line_space, lv_coord_t max_width, uint32_t flag) {
    (void)txt; (void)font; (void)letter_space; (void)line_space; (void)max_width; (void)flag;
    size_res->x = 0;
    size_res->y = 0;
}

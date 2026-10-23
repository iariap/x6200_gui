/*
 * Stubs backing the native CW keyboard dialog idempotence test (issue #217).
 *
 * The REAL src/dialog_cw_kbd.c is staged next to tests/native/stubs_cwkbd/ and
 * compiled against this file: the minimal LVGL surface the dialog uses plus the
 * firmware dependencies it reads (cfg, params, keyer, keyboard, theme). Nothing
 * is rendered and no radio is touched.
 *
 * The instrument is the counter: LVGL invalidates an object every time its label
 * text or a style is set, and the dialog's update_view() runs on a 100 ms timer.
 * The test proves that at rest (no keying, no queue, same theme) the dialog no
 * longer writes a single label or style after the first paint, while every real
 * change (mode, WPM, sent cursor, keyboard, theme, history, queue) still does.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cwkbd_stubs.h"

#include "lvgl/lvgl.h"
#include "styles.h"
#include "theme.h"
#include "keyboard.h"
#include "cw_encoder.h"
#include "buttons.h"
#include "cfg/cfg.h"
#include "params/params.h"
#include "remote_server.h"
#include "textarea_window.h"

/* --- object pool ---------------------------------------------------------- */

#define OBJ_POOL 32
#define LABEL_MAX 16

struct _lv_obj_t {
    char text[2048];
    bool is_textarea;
};

static struct _lv_obj_t s_objs[OBJ_POOL];
static int              s_obj_n;

static lv_obj_t *s_labels[LABEL_MAX];
static int       s_label_n;

/* --- counters ------------------------------------------------------------- */

static int s_label_writes;
static int s_style_writes;
static int s_encoder_sends;
static int s_encoder_stops;
static int s_remote_events;

/* --- stubbed firmware state ----------------------------------------------- */

struct Subject {
    int32_t v;
};

static Subject s_mode;
static Subject s_key_speed;
static Subject s_key_ratio;
static Subject s_key_train;
static Subject s_cw_decoder;
static Subject s_fg_freq;

typedef struct {
    int32_t b;
} cwkbd_unused_t;

cfg_t     cfg;
cfg_cur_t cfg_cur;

/* lv_group_t / lv_timer_t are opaque in the stub header (LVGL keeps them so
 * too), so their storage is raw bytes. */
static char s_group_mem[1];
static char s_timer_mem[1];

lv_group_t   *keyboard_group;

lv_font_t     sony_18;
lv_font_t     sony_20;
lv_font_t     sony_24;
lv_font_t     sony_28;
lv_font_t     sony_30;
theme_shape_t theme_shape;

static uint32_t           s_theme_gen;
static bool               s_keyboard_ready;
static cw_encoder_state_t s_encoder_state;
static int                s_encoder_sent;
static char               s_last_sent[512];

static void (*s_timer_cb)(lv_timer_t *);

/* Opaque cJSON stand-in: the dialog only creates the object and hands it to
 * remote_event(); nothing dereferences it here. */
static cwkbd_unused_t s_json_dummy;

void subject_set_int(Subject *subj, int32_t v) {
    subj->v = v;
}

int32_t subject_get_int(Subject *subj) {
    return subj->v;
}

uint32_t theme_hex(theme_color_t c) {
    uint32_t g = (s_theme_gen * 0x9E3779B1u) >> 8;

    return (g ^ ((uint32_t)c * 0x010101u)) & 0xFFFFFFu;
}

/* --- reset ---------------------------------------------------------------- */

void cwkbd_stub_reset(void) {
    s_obj_n = 0;
    s_label_n = 0;
    memset(s_objs, 0, sizeof(s_objs));
    memset(s_labels, 0, sizeof(s_labels));

    s_label_writes = 0;
    s_style_writes = 0;
    s_encoder_sends = 0;
    s_encoder_stops = 0;
    s_remote_events = 0;

    cfg.key_speed.val   = &s_key_speed;
    cfg.key_ratio.val   = &s_key_ratio;
    cfg.key_train.val   = &s_key_train;
    cfg.cw_decoder.val  = &s_cw_decoder;
    cfg_cur.mode        = &s_mode;
    cfg_cur.fg_freq     = &s_fg_freq;

    s_mode.v = 4;            /* x6100_mode_cw */
    s_key_speed.v = 20;
    s_key_ratio.v = 3;
    s_key_train.v = 0;
    s_cw_decoder.v = 0;
    s_fg_freq.v = 7100000;

    keyboard_group = (lv_group_t *)s_group_mem;
    s_keyboard_ready = false;
    s_theme_gen = 0;
    theme_shape.border = 1;

    s_encoder_state = CW_ENCODER_IDLE;
    s_encoder_sent = 0;
    s_last_sent[0] = 0;

    s_timer_cb = NULL;
}

void cwkbd_stub_counters_reset(void) {
    s_label_writes = 0;
    s_style_writes = 0;
    s_encoder_sends = 0;
    s_encoder_stops = 0;
    s_remote_events = 0;
}

int cwkbd_stub_label_writes(void) {
    return s_label_writes;
}

int cwkbd_stub_style_writes(void) {
    return s_style_writes;
}

int cwkbd_stub_encoder_sends(void) {
    return s_encoder_sends;
}

int cwkbd_stub_encoder_stops(void) {
    return s_encoder_stops;
}

int cwkbd_stub_remote_events(void) {
    return s_remote_events;
}

const char *cwkbd_stub_last_sent(void) {
    return s_last_sent;
}

void cwkbd_stub_timer_tick(void) {
    if (s_timer_cb) {
        s_timer_cb((lv_timer_t *)s_timer_mem);
    }
}

int cwkbd_stub_timer_active(void) {
    return s_timer_cb != NULL;
}

void cwkbd_stub_set_mode(int32_t mode) {
    s_mode.v = mode;
}

void cwkbd_stub_set_wpm(int32_t wpm) {
    s_key_speed.v = wpm;
}

void cwkbd_stub_set_keyboard_ready(bool ready) {
    s_keyboard_ready = ready;
}

void cwkbd_stub_set_encoder(int state, int sent) {
    s_encoder_state = (cw_encoder_state_t)state;
    s_encoder_sent = sent;
}

void cwkbd_stub_set_theme_gen(uint32_t gen) {
    s_theme_gen = gen;
}

int cwkbd_stub_cw_decoder(void) {
    return (int)s_cw_decoder.v;
}

int cwkbd_stub_label_count(void) {
    return s_label_n;
}

const char *cwkbd_stub_label_text(int i) {
    if (i < 0 || i >= s_label_n) {
        return NULL;
    }
    return s_labels[i]->text;
}

/* --- LVGL object surface -------------------------------------------------- */

static lv_obj_t *obj_new(void) {
    if (s_obj_n >= OBJ_POOL) {
        return &s_objs[OBJ_POOL - 1];
    }
    lv_obj_t *o = &s_objs[s_obj_n++];

    memset(o, 0, sizeof(*o));
    return o;
}

lv_color_t lv_color_hex(uint32_t c) {
    lv_color_t col = {.blue = (uint8_t)(c & 0xFF), .green = (uint8_t)((c >> 8) & 0xFF),
                      .red = (uint8_t)((c >> 16) & 0xFF)};

    return col;
}

uint32_t lv_color_to32(lv_color_t color) {
    return ((uint32_t)color.red << 16) | ((uint32_t)color.green << 8) | color.blue;
}

lv_obj_t *lv_obj_create(lv_obj_t *parent) {
    (void)parent;
    return obj_new();
}

void lv_obj_remove_style_all(lv_obj_t *obj) {
    (void)obj;
}

void lv_obj_remove_style(lv_obj_t *obj, void *style, uint32_t selector) {
    (void)obj;
    (void)style;
    (void)selector;
}

void lv_obj_set_size(lv_obj_t *obj, int32_t w, int32_t h) {
    (void)obj;
    (void)w;
    (void)h;
}

void lv_obj_set_width(lv_obj_t *obj, int32_t w) {
    (void)obj;
    (void)w;
}

void lv_obj_set_pos(lv_obj_t *obj, int32_t x, int32_t y) {
    (void)obj;
    (void)x;
    (void)y;
}

void lv_obj_align(lv_obj_t *obj, int align, int32_t x, int32_t y) {
    (void)obj;
    (void)align;
    (void)x;
    (void)y;
}

void lv_obj_clear_flag(lv_obj_t *obj, uint32_t flag) {
    (void)obj;
    (void)flag;
}

void lv_obj_add_state(lv_obj_t *obj, uint32_t state) {
    (void)obj;
    (void)state;
}

void lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, int filter, void *user_data) {
    (void)obj;
    (void)cb;
    (void)filter;
    (void)user_data;
}

void lv_obj_set_style_text_font(lv_obj_t *obj, const lv_font_t *font, uint32_t part) {
    (void)obj;
    (void)font;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t color, uint32_t part) {
    (void)obj;
    (void)color;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_text_align(lv_obj_t *obj, int align, uint32_t part) {
    (void)obj;
    (void)align;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t color, uint32_t part) {
    (void)obj;
    (void)color;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_bg_opa(lv_obj_t *obj, uint8_t opa, uint32_t part) {
    (void)obj;
    (void)opa;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_border_color(lv_obj_t *obj, lv_color_t color, uint32_t part) {
    (void)obj;
    (void)color;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_border_width(lv_obj_t *obj, int32_t width, uint32_t part) {
    (void)obj;
    (void)width;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_radius(lv_obj_t *obj, int32_t radius, uint32_t part) {
    (void)obj;
    (void)radius;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_pad_all(lv_obj_t *obj, int32_t pad, uint32_t part) {
    (void)obj;
    (void)pad;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_pad_hor(lv_obj_t *obj, int32_t pad, uint32_t part) {
    (void)obj;
    (void)pad;
    (void)part;
    s_style_writes++;
}

void lv_obj_set_style_pad_top(lv_obj_t *obj, int32_t pad, uint32_t part) {
    (void)obj;
    (void)pad;
    (void)part;
    s_style_writes++;
}

lv_obj_t *lv_label_create(lv_obj_t *parent) {
    (void)parent;
    lv_obj_t *o = obj_new();

    if (s_label_n < LABEL_MAX) {
        s_labels[s_label_n++] = o;
    }
    return o;
}

void lv_label_set_text(lv_obj_t *obj, const char *text) {
    s_label_writes++;
    snprintf(obj->text, sizeof(obj->text), "%s", text ? text : "");
}

void lv_label_set_text_fmt(lv_obj_t *obj, const char *fmt, ...) {
    va_list ap;

    s_label_writes++;
    va_start(ap, fmt);
    vsnprintf(obj->text, sizeof(obj->text), fmt, ap);
    va_end(ap);
}

void lv_label_set_recolor(lv_obj_t *obj, bool en) {
    (void)obj;
    (void)en;
}

void lv_label_set_long_mode(lv_obj_t *obj, int mode) {
    (void)obj;
    (void)mode;
}

const char *lv_label_get_text(lv_obj_t *obj) {
    return obj->text;
}

lv_obj_t *lv_textarea_create(lv_obj_t *parent) {
    (void)parent;
    lv_obj_t *o = obj_new();

    o->is_textarea = true;
    return o;
}

const char *lv_textarea_get_text(lv_obj_t *obj) {
    return obj->text;
}

void lv_textarea_set_text(lv_obj_t *obj, const char *text) {
    snprintf(obj->text, sizeof(obj->text), "%s", text ? text : "");
}

void lv_textarea_add_text(lv_obj_t *obj, const char *text) {
    size_t n = strlen(obj->text);

    snprintf(obj->text + n, sizeof(obj->text) - n, "%s", text ? text : "");
}

void lv_textarea_del_char(lv_obj_t *obj) {
    size_t n = strlen(obj->text);

    if (n) {
        obj->text[n - 1] = 0;
    }
}

void lv_textarea_set_one_line(lv_obj_t *obj, bool en) {
    (void)obj;
    (void)en;
}

void lv_textarea_set_max_length(lv_obj_t *obj, uint32_t max) {
    (void)obj;
    (void)max;
}

void lv_textarea_set_placeholder_text(lv_obj_t *obj, const char *text) {
    (void)obj;
    (void)text;
}

void lv_group_add_obj(lv_group_t *group, lv_obj_t *obj) {
    (void)group;
    (void)obj;
}

void lv_group_remove_obj(lv_obj_t *obj) {
    (void)obj;
}

void lv_group_focus_obj(lv_obj_t *obj) {
    (void)obj;
}

void lv_group_set_editing(lv_group_t *group, bool edit) {
    (void)group;
    (void)edit;
}

lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *), uint32_t period, void *user_data) {
    (void)period;
    (void)user_data;
    s_timer_cb = cb;
    return (lv_timer_t *)s_timer_mem;
}

void lv_timer_del(lv_timer_t *timer) {
    (void)timer;
    s_timer_cb = NULL;
}

lv_indev_t *lv_indev_get_act(void) {
    return NULL;
}

lv_indev_type_t lv_indev_get_type(lv_indev_t *indev) {
    (void)indev;
    return LV_INDEV_TYPE_NONE;
}

void *lv_event_get_param(lv_event_t *e) {
    (void)e;
    return NULL;
}

/* --- dialog machinery ----------------------------------------------------- */

lv_obj_t *dialog_init(lv_obj_t *parent) {
    (void)parent;
    return obj_new();
}

void dialog_destruct(void) {
}

/* --- firmware dependencies ------------------------------------------------ */

int params_msg_cw_list(char (*out)[PARAMS_MSG_CW_LEN], int max) {
    (void)out;
    (void)max;
    return 0;   /* the msg_cw table ships empty */
}

void buttons_load_page(buttons_page_t *page) {
    (void)page;
}

void button_next_page_cb(button_item_t *item) {
    (void)item;
}

void msg_update_text_fmt(const char *fmt, ...) {
    (void)fmt;
}

/* --- on-screen keyboard window -------------------------------------------- */

static char s_osk_text[64];

lv_obj_t *textarea_window_open(textarea_window_cb_t ok_cb, textarea_window_cb_t cancel_cb) {
    (void)ok_cb;
    (void)cancel_cb;
    s_osk_text[0] = 0;
    return obj_new();
}

void textarea_window_close(void) {
}

const char *textarea_window_get(void) {
    return s_osk_text;
}

uint16_t radio_change_vol(int16_t df) {
    (void)df;
    return 0;
}

int cw_get_wpm(void) {
    return (int)s_key_speed.v;
}

void keyboard_init(void) {
}

bool keyboard_ready(void) {
    return s_keyboard_ready;
}

void cw_encoder_send(const char *text, bool beacon) {
    (void)beacon;
    s_encoder_sends++;
    snprintf(s_last_sent, sizeof(s_last_sent), "%s", text ? text : "");
    s_encoder_state = CW_ENCODER_SEND;
}

void cw_encoder_stop(void) {
    s_encoder_stops++;
    s_encoder_state = CW_ENCODER_IDLE;
    s_encoder_sent = 0;
}

cw_encoder_state_t cw_encoder_state(void) {
    return s_encoder_state;
}

int cw_encoder_sent(void) {
    return s_encoder_sent;
}

cJSON *cJSON_CreateObject(void) {
    return (cJSON *)&s_json_dummy;
}

void cJSON_AddStringToObject(cJSON *o, const char *name, const char *s) {
    (void)o;
    (void)name;
    (void)s;
}

void remote_event(cJSON *o) {
    (void)o;
    s_remote_events++;
}

/*
 * Stubs for the native dialog restore test (issue #94).
 *
 * They stand in for everything src/dialog.c touches, and — the point of the
 * test — they carry a REAL radio_state_save/radio_state_restore (the same
 * snapshot radio.c keeps: dial, mode, agc/pre/att and power over the cfg_cur
 * subjects). The SQLite-backed cfg_memory_save/load the pre-fix dialog.c used
 * is stubbed as a no-op: the native test has no database, so the old path
 * cannot restore anything, and the test only goes green once the backup lives
 * in the in-memory radio-state.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>
#include <stdlib.h>

#include "cfg/subjects.h"
#include "buttons.h"
#include "styles.h"
#include "keyboard.h"
#include "events.h"
#include "waterfall.h"
#include "main_screen.h"
#include "cluster_column.h"
#include "radio.h"

/* ---- subjects: a tiny int store -------------------------------------- */

struct Subject {
    int32_t val;
};

Subject *subject_create_int(int32_t val) {
    Subject *s = malloc(sizeof(*s));
    s->val = val;
    return s;
}

int32_t subject_get_int(Subject *subj) {
    return subj->val;
}

void subject_set_int(Subject *subj, int32_t val) {
    subj->val = val;
}

/* ---- the radio state (what radio.c keeps) ----------------------------- */

cfg_cur_t cfg_cur;
cfg_t     cfg;

static struct {
    int32_t freq;
    int32_t mode;
    int32_t agc;
    int32_t pre;
    int32_t att;
    int32_t pwr;
    bool    valid;
} radio_backup;

void radio_state_save(void) {
    radio_backup.freq  = subject_get_int(cfg_cur.fg_freq);
    radio_backup.mode  = subject_get_int(cfg_cur.mode);
    radio_backup.agc   = subject_get_int(cfg_cur.agc);
    radio_backup.pre   = subject_get_int(cfg_cur.pre);
    radio_backup.att   = subject_get_int(cfg_cur.att);
    radio_backup.pwr   = subject_get_int(cfg.pwr.val);
    radio_backup.valid = true;
}

void radio_state_restore(void) {
    if (!radio_backup.valid) {
        return;
    }
    radio_backup.valid = false;

    subject_set_int(cfg_cur.fg_freq, radio_backup.freq);
    subject_set_int(cfg_cur.mode, radio_backup.mode);
    subject_set_int(cfg_cur.agc, radio_backup.agc);
    subject_set_int(cfg_cur.pre, radio_backup.pre);
    subject_set_int(cfg_cur.att, radio_backup.att);
    subject_set_int(cfg.pwr.val, radio_backup.pwr);
}

/* ---- the SQLite-backed backup the pre-fix code called (no database here) */

bool cfg_memory_load(int32_t id) {
    (void) id;
    return false;
}

void cfg_memory_save(int32_t id) {
    (void) id;
}

/* ---- the rest of src/dialog.c's surface ------------------------------ */

lv_style_t dialog_style;
lv_style_t dialog_item_style;
lv_style_t dialog_item_focus_style;
lv_style_t dialog_item_edited_style;

lv_group_t *keyboard_group;

static buttons_page_t page;

buttons_page_t *buttons_get_cur_page(void) {
    return &page;
}
void buttons_unload_page(void) {
}
void buttons_load_page(buttons_page_t *p) {
    (void) p;
}

void waterfall_refresh_period_set(uint8_t period) {
    (void) period;
}
void waterfall_refresh_reset(void) {
}
void main_screen_keys_enable(bool enable) {
    (void) enable;
}
void cluster_column_hide(bool hide) {
    (void) hide;
}
void event_send(lv_obj_t *obj, lv_event_code_t code, void *param) {
    (void) obj;
    (void) code;
    (void) param;
}

lv_obj_t *lv_obj_create(lv_obj_t *parent) {
    (void) parent;
    return NULL;
}
void lv_obj_del(lv_obj_t *obj) {
    (void) obj;
}
void lv_obj_remove_style_all(lv_obj_t *obj) {
    (void) obj;
}
void lv_obj_add_style(lv_obj_t *obj, const lv_style_t *style, uint32_t selector) {
    (void) obj;
    (void) style;
    (void) selector;
}
void lv_obj_clear_flag(lv_obj_t *obj, uint32_t flag) {
    (void) obj;
    (void) flag;
}
void lv_obj_add_flag(lv_obj_t *obj, uint32_t flag) {
    (void) obj;
    (void) flag;
}
void lv_obj_add_event_cb(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t filter, void *user_data) {
    (void) obj;
    (void) cb;
    (void) filter;
    (void) user_data;
}
void lv_group_add_obj(lv_group_t *group, lv_obj_t *obj) {
    (void) group;
    (void) obj;
}
void lv_obj_set_style_bg_opa(lv_obj_t *obj, uint8_t opa, uint32_t part) {
    (void) obj;
    (void) opa;
    (void) part;
}
void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t color, uint32_t part) {
    (void) obj;
    (void) color;
    (void) part;
}
void lv_obj_set_style_border_width(lv_obj_t *obj, lv_coord_t width, uint32_t part) {
    (void) obj;
    (void) width;
    (void) part;
}
void lv_obj_set_style_border_color(lv_obj_t *obj, lv_color_t color, uint32_t part) {
    (void) obj;
    (void) color;
    (void) part;
}
lv_color_t lv_color_white(void) {
    lv_color_t c = {0};
    return c;
}
lv_color_t lv_color_black(void) {
    lv_color_t c = {0};
    return c;
}

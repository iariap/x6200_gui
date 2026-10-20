/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Banda contextual del DFL (issue #183; ver src/dfl_context.h).
 *
 *  Geometria derivada del waterfall real (no hardcodea 240): el chip se apoya en
 *  el borde superior derecho, con 4 px de margen, y mide a lo sumo 340x40 para
 *  no tapar senales del centro del waterfall.
 */

#include "dfl_context.h"

#include "styles.h"
#include "theme.h"

#define DFL_CTX_MARGIN   6
#define DFL_CTX_H        40
#define DFL_CTX_MAX_W    340
#define DFL_CTX_ACTION_W 128

static lv_obj_t *band;
static lv_obj_t *text_label;
static lv_obj_t *action_btn;
static lv_obj_t *action_label;

static void (*action_cb)(void);

static void action_event_cb(lv_event_t *e) {
    (void) e;

    if (action_cb) {
        action_cb();
    }
}

void dfl_context_init(lv_obj_t *waterfall_widget) {
    if (band != NULL || waterfall_widget == NULL) {
        return;
    }

    lv_obj_t *parent = lv_obj_get_parent(waterfall_widget);

    lv_obj_update_layout(waterfall_widget);

    lv_area_t wa;

    lv_obj_get_coords(waterfall_widget, &wa);

    lv_coord_t wf_w = wa.x2 - wa.x1 + 1;

    band = lv_obj_create(parent);
    lv_obj_remove_style_all(band);
    lv_obj_set_size(band, DFL_CTX_MAX_W, DFL_CTX_H);
    lv_obj_set_pos(band, wa.x1 + wf_w - DFL_CTX_MAX_W - DFL_CTX_MARGIN, wa.y1 + DFL_CTX_MARGIN);
    lv_obj_set_style_radius(band, 4, 0);
    lv_obj_set_style_bg_color(band, TC(chip), 0);
    lv_obj_set_style_bg_opa(band, LV_OPA_80, 0);
    lv_obj_set_style_border_width(band, 1, 0);
    lv_obj_set_style_border_color(band, TC(panel_border), 0);
    lv_obj_set_style_pad_all(band, 0, 0);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(band, LV_OBJ_FLAG_HIDDEN);

    text_label = lv_label_create(band);
    lv_obj_set_style_text_font(text_label, &sony_18, 0);
    lv_obj_set_style_text_color(text_label, TC(text), 0);
    lv_obj_set_pos(text_label, 10, 11);
    lv_obj_set_width(text_label, DFL_CTX_MAX_W - 20);
    lv_label_set_text(text_label, "");
    lv_obj_set_user_data(text_label, (void *) (intptr_t) DFL_CTX_LABEL);

    action_btn = lv_obj_create(band);
    lv_obj_remove_style_all(action_btn);
    lv_obj_set_size(action_btn, DFL_CTX_ACTION_W, DFL_CTX_H - 8);
    lv_obj_set_pos(action_btn, DFL_CTX_MAX_W - DFL_CTX_ACTION_W - 4, 4);
    lv_obj_set_style_radius(action_btn, 4, 0);
    lv_obj_set_style_bg_color(action_btn, TC(sel_bg), 0);
    lv_obj_set_style_bg_opa(action_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(action_btn, 1, 0);
    lv_obj_set_style_border_color(action_btn, TC(sel_border), 0);
    lv_obj_add_flag(action_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(action_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(action_btn, (void *) (intptr_t) DFL_CTX_ACTION);
    lv_obj_add_event_cb(action_btn, action_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(action_btn, LV_OBJ_FLAG_HIDDEN);

    action_label = lv_label_create(action_btn);
    lv_obj_set_style_text_font(action_label, &sony_18, 0);
    lv_obj_set_style_text_color(action_label, TC(sel_text), 0);
    lv_label_set_text(action_label, "Cancelar");
    lv_obj_center(action_label);
}

void dfl_context_shutdown(void) {
    band         = NULL;
    text_label   = NULL;
    action_btn   = NULL;
    action_label = NULL;
    action_cb    = NULL;
}

void dfl_context_show_text(const char *text) {
    if (band == NULL) {
        return;
    }

    action_cb = NULL;
    lv_label_set_text(text_label, text ? text : "");
    lv_obj_add_flag(action_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_HIDDEN);
}

void dfl_context_show_action(const char *text, const char *action, void (*cb)(void)) {
    if (band == NULL) {
        return;
    }

    action_cb = cb;
    lv_label_set_text(text_label, text ? text : "");
    lv_label_set_text(action_label, action ? action : "");
    lv_obj_clear_flag(action_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_HIDDEN);
}

void dfl_context_hide(void) {
    if (band == NULL) {
        return;
    }

    action_cb = NULL;
    lv_obj_add_flag(band, LV_OBJ_FLAG_HIDDEN);
}

bool dfl_context_visible(void) {
    return band != NULL && !lv_obj_has_flag(band, LV_OBJ_FLAG_HIDDEN);
}

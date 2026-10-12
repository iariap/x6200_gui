/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - About Dialog
 *
 *  Copyright (c) 2024 LU2DSX
 */

#include "dialog_about.h"
#include "main.h"

#include "dialog.h"
#include "styles.h"
#include "events.h"
#include "keyboard.h"
#include "main_screen.h"

/* The firmware version, from main.h: it used to be a hardcoded "2.0-opt" */
#define APP_VERSION VERSION

static void construct_cb(lv_obj_t *parent);
static void destruct_cb();
static void key_cb(lv_event_t * e);

static dialog_t             dialog = {
    .run = false,
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .audio_cb = NULL,
    .key_cb = key_cb
};

dialog_t                    *dialog_about = &dialog;

static lv_obj_t             *title_label;
static lv_obj_t             *version_label;
static lv_obj_t             *author_label;
static lv_obj_t             *heart_label;

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    /* Container for centered content within the full-width dialog */
    lv_obj_t *cont = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cont);

    /* Make the container focusable so key events are delivered */
    lv_group_add_obj(keyboard_group, cont);
    lv_obj_add_event_cb(cont, key_cb, LV_EVENT_KEY, NULL);

    // Title
    title_label = lv_label_create(cont);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &sony_44, 0);
    lv_label_set_text(title_label, "About");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Version
    version_label = lv_label_create(cont);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(version_label, &sony_20, 0);
    lv_label_set_text_fmt(version_label, "Version %s", APP_VERSION);
    lv_obj_align(version_label, LV_ALIGN_CENTER, 0, -30);

    // Callsign
    heart_label = lv_label_create(cont);
    lv_obj_set_style_text_color(heart_label, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_text_font(heart_label, &sony_24, 0);
    lv_label_set_text(heart_label, "73 de LU2DSX");
    lv_obj_align(heart_label, LV_ALIGN_CENTER, 0, 20);

    // Author text
    author_label = lv_label_create(cont);
    lv_obj_set_style_text_color(author_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(author_label, &sony_18, 0);
    lv_label_set_text(author_label, "Made with love by LU2DSX");
    lv_obj_align(author_label, LV_ALIGN_CENTER, 0, 70);
}

static void destruct_cb() {
    dialog.obj = NULL;
}

static void key_cb(lv_event_t * e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
        case LV_KEY_ENTER:
            dialog_destruct(&dialog);
            break;

        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;

        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
    }
}

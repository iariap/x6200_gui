/*
 * Minimal dialog stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c keeps one dialog_t and its construct/destruct callbacks are
 * what the test drives (the firmware's own entry: dialog_construct() ->
 * construct_cb()). The struct keeps the members the dialog's designated
 * initialiser sets.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>

#include "buttons.h"
#include "lvgl/lvgl.h"

typedef void (*dialog_construct_cb_t)(lv_obj_t *);
typedef void (*dialog_destruct_cb_t)(void);
typedef void (*dialog_audio_cb_t)(unsigned int n, void *samples);

typedef struct {
    lv_obj_t              *obj;
    dialog_construct_cb_t  construct_cb;
    dialog_destruct_cb_t   destruct_cb;
    dialog_audio_cb_t      audio_cb;
    buttons_page_t        *btn_page;
    lv_event_cb_t          key_cb;
    bool                   run;
} dialog_t;

lv_obj_t *dialog_init(lv_obj_t *parent);
void      dialog_destruct(void);

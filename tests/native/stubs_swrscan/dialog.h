/*
 * Minimal dialog stub for the native SWR Scan dialog test (issue #85).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "buttons.h"
#include "lvgl/lvgl.h"

typedef void (*dialog_construct_cb_t)(lv_obj_t *);
typedef void (*dialog_destruct_cb_t)(void);
typedef void (*dialog_audio_cb_t)(unsigned int n, void *samples);
typedef void (*dialog_rotary_cb_t)(int32_t diff);

typedef struct {
    lv_obj_t              *obj;
    dialog_construct_cb_t  construct_cb;
    dialog_destruct_cb_t   destruct_cb;
    dialog_audio_cb_t      audio_cb;
    dialog_rotary_cb_t     rotary_cb;
    buttons_page_t        *btn_page;
    buttons_page_t        *prev_page;
    lv_event_cb_t          key_cb;
    bool                   restore_radio;
    bool                   run;
} dialog_t;

lv_obj_t *dialog_init(lv_obj_t *parent);
void      dialog_construct(dialog_t *dialog, lv_obj_t *parent);
void      dialog_destruct();

/* #133: la guarda dura del control remoto (la ventana abierta es esta y esta viva). El stub
 * la resuelve con el dialogo "actual" que el test marca abierto/cerrado. */
bool      dialog_type_is_run(dialog_t *dialog);

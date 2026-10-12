/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#define BUTTONS 5

#ifdef __cplusplus
#include "cfg/subjects.h"

extern "C" {
#endif

#include "lvgl/lvgl.h"
#include "params/params.h"
#include "main_screen.h"
#include "mfk.h"
#include "vol.h"

typedef enum {
    BTN_EMPTY,
    BTN_TEXT,
    BTN_TEXT_FN,
} btn_type_t;

typedef struct button_item_t {
    /* Fields the ported X6100 app dialogs set (see button_data_t below) */
    bool                   mark;
    bool                   disabled;
    btn_type_t type;
    union {
        const char *label;
        const char *(*label_fn)();
    };
    void (*press)(struct button_item_t *);
    void (*hold)(struct button_item_t *);
    const char *voice;
    // next/prev page
    struct buttons_page_t *next;
    struct buttons_page_t *prev;
    union {
        int32_t            data;        /* this GUI's name for it */
        int32_t            ctrl;        /* the X6100 name the ported dialogs use */
    };
    /* The X6100 lets the encoder change the value of the focused app button
     * (WeFax align/tilt); accepted here, the registry is not ported yet */
    bool                   encoder_allowed;
    union {
        lv_obj_t          *label_obj;   /* this GUI's name for it */
        lv_obj_t          *disp_btn;    /* the X6100 name the ported dialogs use */
    };
    Subject               **subj;
    Observer              *observer;
} button_item_t;

typedef struct buttons_page_t {
    button_item_t *items[BUTTONS];
} buttons_page_t;

typedef buttons_page_t *buttons_group_t[];

/* The X6100 firmware renamed button_item_t to button_data_t when it added the
 * app button pages; the layout is the same, so the ported dialogs use theirs */
typedef button_item_t button_data_t;

extern buttons_page_t buttons_page_vol_1;

extern buttons_page_t buttons_page_msg_cw_1;
extern buttons_page_t buttons_page_msg_cw_2;

extern buttons_page_t buttons_page_rtty;

extern buttons_group_t buttons_group_gen;
extern buttons_group_t buttons_group_app;
extern buttons_group_t buttons_group_key;
extern buttons_group_t buttons_group_dfn;
extern buttons_group_t buttons_group_dfl;

// TODO: move to applications
extern buttons_group_t buttons_group_msg_cw;
extern buttons_group_t buttons_group_msg_voice;

void            buttons_init(lv_obj_t *parent);
void            buttons_refresh(button_item_t *item);
void            buttons_mark(button_item_t *item, bool on);
void            buttons_disabled(button_item_t *item, bool on);
void            buttons_load(uint8_t n, button_item_t *item);
void            buttons_load_page(buttons_page_t *page);
void            buttons_unload_page();
void            button_next_page_cb(button_item_t *item);
void            button_prev_page_cb(button_item_t *item);
void            button_update_label(button_item_t *item, char *label);
void            buttons_press(uint8_t n, bool hold);
void            buttons_load_page_group(buttons_group_t group);
buttons_page_t *buttons_get_cur_page();

#ifdef __cplusplus
}
#endif

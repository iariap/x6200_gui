/*
 * Minimal buttons stub for the native SWR Scan dialog test (issue #85).
 *
 * button_item_t keeps the REAL layout (src/buttons.h) because the crash was a
 * NULL item dereferenced at item->type (offset 4): the test drives the dialog's
 * callback with a real item and with NULL, so the layout must be faithful.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "lvgl/lvgl.h"

typedef struct Subject Subject;

typedef enum {
    BTN_EMPTY,
    BTN_TEXT,
    BTN_TEXT_FN,
} btn_type_t;

typedef struct button_item_t {
    bool                   mark;
    bool                   disabled;
    btn_type_t             type;
    union {
        const char *label;
        const char *(*label_fn)();
    };
    void (*press)(struct button_item_t *);
    void (*hold)(struct button_item_t *);
    const char *voice;
    struct buttons_page_t *next;
    struct buttons_page_t *prev;
    union {
        int32_t data;
        int32_t ctrl;
    };
    bool    encoder_allowed;
    union {
        lv_obj_t *label_obj;
        lv_obj_t *disp_btn;
    };
    Subject **subj;
    void  *observer;
} button_item_t;

typedef button_item_t button_data_t;

typedef struct buttons_page_t {
    button_item_t *items[5];
} buttons_page_t;

/* The real one dereferences item->type (that is the #85 crash). The test stub
 * records the NULL case instead of crashing, so the failure is an assertion. */
void button_update_label(button_item_t *item, char *label);

void buttons_unload_page(void);
void buttons_load_page(buttons_page_t *page);

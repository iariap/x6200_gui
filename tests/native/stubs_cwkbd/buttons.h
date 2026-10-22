/*
 * Minimal buttons stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c builds its pages (Send/Stop/Clear/Close and the MSG cells)
 * and loads the first one. The struct keeps the members the dialog sets: type,
 * label, press and next.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>

typedef enum {
    BTN_EMPTY,
    BTN_TEXT,
    BTN_TEXT_FN,
} btn_type_t;

typedef struct button_item_t {
    btn_type_t type;
    const char *label;
    void (*press)(struct button_item_t *);
    struct buttons_page_t *next;
} button_item_t;

typedef struct buttons_page_t {
    button_item_t *items[5];
} buttons_page_t;

typedef button_item_t button_data_t;

void buttons_load_page(buttons_page_t *page);
void button_next_page_cb(button_item_t *item);

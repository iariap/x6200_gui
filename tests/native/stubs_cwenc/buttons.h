/*
 * Minimal buttons stub for the native CW encoder test (issue #116).
 *
 * src/cw_encoder.c ends a message by unloading the page and loading
 * buttons_page_msg_cw_1 (defined in src/dialog_msg_cw.c, faked here).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef struct button_item_t {
    void (*press)(struct button_item_t *);
} button_item_t;

typedef struct buttons_page_t {
    button_item_t *items[5];
} buttons_page_t;

extern buttons_page_t buttons_page_msg_cw_1;

void buttons_unload_page(void);
void buttons_load_page(buttons_page_t *page);

/* Minimal buttons stub for the native dialog restore test (issue #94).
 * src/dialog.c only stores the app's button page and restores the previous one.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "lvgl/lvgl.h"
typedef struct buttons_page_t {
    int unused;
} buttons_page_t;
buttons_page_t *buttons_get_cur_page(void);
void buttons_unload_page(void);
void buttons_load_page(buttons_page_t *page);

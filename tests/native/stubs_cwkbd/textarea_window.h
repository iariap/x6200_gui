/*
 * Minimal textarea_window stub for the native CW keyboard dialog test (#217).
 *
 * src/dialog_cw_kbd.c opens the on-screen keyboard (MFK press with an empty input)
 * and reads its text. tests/native/cwkbd_stubs.c owns the (empty) state.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>

#include "lvgl/lvgl.h"

typedef bool (*textarea_window_cb_t)(void);

lv_obj_t *   textarea_window_open(textarea_window_cb_t ok_cb, textarea_window_cb_t cancel_cb);
void         textarea_window_close(void);
const char * textarea_window_get(void);

/*
 * Minimal keyboard stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c adds its root to keyboard_group and reads keyboard_ready()
 * for the header line. tests/native/cwkbd_stubs.c owns both.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>

#include "lvgl/lvgl.h"

extern lv_group_t *keyboard_group;

void keyboard_init(void);
bool keyboard_ready(void);

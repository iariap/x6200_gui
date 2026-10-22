/*
 * keyboard.h stub for the native hkey test (issue #131): src/hkey.c only needs
 * the group (lv_group_get_editing / event_send_key -> lv_group_get_focused).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "lvgl/lvgl.h"

extern lv_group_t *keyboard_group;

/*
 * keyboard.h stub for the native events-queue test (issue #111): src/events.c
 * only needs the group the focused object belongs to (event_send_key).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "lvgl/lvgl.h"

extern lv_group_t *keyboard_group;

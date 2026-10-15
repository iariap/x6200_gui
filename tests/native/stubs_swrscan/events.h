/*
 * Minimal events stub for the native SWR Scan dialog test (issue #85).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "lvgl/lvgl.h"

/* src/events.h: the four volume keys the SWR Scan key callback handles. */
enum {
    KEY_VOL_LEFT_EDIT = 0x290,
    KEY_VOL_RIGHT_EDIT,
    KEY_VOL_LEFT_SELECT,
    KEY_VOL_RIGHT_SELECT,
};

void event_send(lv_obj_t *obj, lv_event_code_t event_code, void *param);

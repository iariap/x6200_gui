/* Minimal events stub for the native dialog restore test (issue #94).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "lvgl/lvgl.h"
void event_send(lv_obj_t *obj, lv_event_code_t code, void *param);

/*
 * Minimal msg.h for the WiFi idle-pump native stage (issue #218, F-06).
 * Only the function src/wifi.cpp uses (no more): the stage records the last text the GUI
 * would show, so the messages the module already emitted ("Wrong WiFi password") can be
 * checked without a screen.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

/* Como el msg.h de la ROM: de aca sale lv_timer_t/lv_msg_send (via lvgl/lvgl.h). */
#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void msg_update_text_fmt(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

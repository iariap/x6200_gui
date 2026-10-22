/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_cw_kbd;

/* Web remote (X6100 Link): an app command (LVGL thread) and the "ax" state object (any thread) */
void dialog_cw_kbd_remote(const char *cmd, int32_t val, const char *text);
int  dialog_cw_kbd_remote_state(char *buf, size_t size);

#ifdef __cplusplus
}
#endif

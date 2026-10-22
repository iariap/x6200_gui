/*
 * Minimal params stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c fills its MSG pages from params_msg_cw_list(). The stub
 * returns no messages (the msg_cw table ships empty), so the dialog keeps its
 * single CW KBD page exactly as on a fresh radio.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#define PARAMS_MSG_CW_LEN 65

int params_msg_cw_list(char (*out)[PARAMS_MSG_CW_LEN], int max);

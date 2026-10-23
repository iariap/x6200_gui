/*
 * Minimal msg stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c shows "Set CW mode to transmit" through it when a line is
 * enqueued outside CW mode.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

void msg_update_text_fmt(const char *fmt, ...);

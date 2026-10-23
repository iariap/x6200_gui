/*
 * Minimal cw stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c reports cw_get_wpm() in the web remote's state object; the
 * decoder itself (src/cw.cpp) is not part of this test.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

int cw_get_wpm(void);

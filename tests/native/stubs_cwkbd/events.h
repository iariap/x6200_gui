/*
 * Minimal events stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c switches on the volume keys of events.h; only those four
 * constants are needed.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef enum {
    KEY_VOL_LEFT_EDIT      = 0x290,
    KEY_VOL_RIGHT_EDIT,
    KEY_VOL_LEFT_SELECT,
    KEY_VOL_RIGHT_SELECT,
} keys_t;

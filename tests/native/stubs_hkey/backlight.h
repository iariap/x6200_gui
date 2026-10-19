/*
 * backlight.h stub for the native hkey test (issue #131): src/hkey.c calls
 * backlight_tick() on every key press, and that call is another LVGL touch
 * (it creates/resets the idle timer). The stub counts it and records the
 * calling thread so the test can pin the deferral.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

void backlight_tick(void);

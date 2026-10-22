/*
 * Minimal voice.h for the native backlight test: backlight_switch() (not the
 * path under test, but linked) announces the display state.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

void voice_say_text_fmt(const char * fmt, ...);

/*
 * Minimal styles stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c only needs DIALOG_H (the dialog area it lays its widgets
 * out in) and the five Sony fonts it names. The real styles.h pulls in the whole
 * style/theme surface, which this stub does not need.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "lvgl/lvgl.h"

#define DIALOG_H 372

extern lv_font_t sony_18;
extern lv_font_t sony_20;
extern lv_font_t sony_24;
extern lv_font_t sony_28;
extern lv_font_t sony_30;

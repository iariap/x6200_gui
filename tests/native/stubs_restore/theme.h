/* Minimal theme stub for the native dialog restore test (issue #94).
 *
 * src/dialog.c asks the theme for the colour role of the textarea cursor
 * (`TC(text)`, issue #287) instead of writing a fixed colour. The restore test
 * does not render: the role resolves to a fixed colour and the test keeps
 * checking the radio-state restore, which is what it exists for. Every other
 * symbol of src/theme.h is unused by dialog.c.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "lvgl/lvgl.h"
#define TC(role) lv_color_white()

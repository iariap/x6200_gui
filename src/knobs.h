/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - what the VOL and MFK encoders are controlling.
 *
 *  The X6100 has a knobs module that maintains this state; this GUI's main
 *  screen draws its own boxes, so here it is a read-only view over our vol.c and
 *  mfk.cpp for the apps that draw the knobs themselves (Field Ops).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool knobs_visible(void);

/* The X6100 hides the knob labels while a dialog is open. This GUI's dialogs
 * cover that area, so there is nothing to hide: accepted and ignored. */
void knobs_display(bool on);

/* What VOL (vol true) or MFK controls: its name, its value as text, a level
 * 0..1 (-1 when it has no scale) and whether turning it changes the value
 * (false: it chooses the control). */
bool knobs_info(bool vol, const char **name, char *val, size_t size, float *level, bool *edit);

#ifdef __cplusplus
}
#endif

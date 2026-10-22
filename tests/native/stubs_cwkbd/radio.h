/*
 * Minimal radio stub for the native CW keyboard dialog test (issue #217).
 *
 * src/dialog_cw_kbd.c maps the volume keys to radio_change_vol().
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

uint16_t radio_change_vol(int16_t df);

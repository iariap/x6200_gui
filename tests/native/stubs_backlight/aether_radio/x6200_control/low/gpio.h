/*
 * Minimal aether GPIO header for the native backlight test: src/backlight.c
 * names the buttons-light pin. The real one comes from the radio SDK, which the
 * host build does not have.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

enum {
    x6200_pin_light = 0,
};

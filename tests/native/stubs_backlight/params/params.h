/*
 * Minimal params.h for the native backlight test.
 *
 * Only the fields src/backlight.c reads and writes (the brightness level,
 * timeout and buttons) plus the lock helpers, so the real params.h (SQLite +
 * the whole LVGL) is not compiled here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BUTTONS_DARK = 0,
    BUTTONS_LIGHT,
    BUTTONS_TEMPORARILY
} buttons_light_t;

typedef struct {
    bool brightness_normal;
    bool brightness_idle;
    bool brightness_timeout;
    bool brightness_buttons;
} params_dirty_t;

typedef struct {
    int16_t           brightness_normal;
    int16_t           brightness_idle;
    uint16_t          brightness_timeout;
    buttons_light_t   brightness_buttons;
    params_dirty_t    dirty;
} params_t;

extern params_t params;

void params_lock(void);
void params_unlock(bool *dirty);

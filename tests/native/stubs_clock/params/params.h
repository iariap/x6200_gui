/*
 * Minimal params.h for the native clock test.
 *
 * Only the fields src/clock.c reads (the clock view and its timeouts) and the
 * two lock helpers, so the real params.h (which loads SQLite and pulls the whole
 * LVGL) is not compiled here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "clock.h"

typedef struct {
    bool clock_view;
    bool clock_time_timeout;
    bool clock_power_timeout;
    bool clock_tx_timeout;
} params_dirty_t;

typedef struct {
    clock_view_t clock_view;
    uint8_t clock_time_timeout;
    uint8_t clock_power_timeout;
    uint8_t clock_tx_timeout;
    params_dirty_t dirty;
} params_t;

extern params_t params;

void params_lock(void);
void params_unlock(bool *dirty);

/*
 *  Parameter store for the host UI harness.
 *
 *  The firmware loads params_t from SQLite (src/params/params.c). The harness
 *  does not: it defines the same global with the values that decide the
 *  rendered layout, and the few setters the components may call. No database,
 *  no file is read or written.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "params/params.h"
#include "params/common.h"
#include "mock_harness.h"

params_t params;

pthread_mutex_t params_mux = PTHREAD_MUTEX_INITIALIZER;

void mock_params_init(void)
{
    params.clock_view          = CLOCK_POWER_ALLWAYS; /* battery, no wall clock */
    params.clock_time_timeout  = 10;
    params.clock_power_timeout = 5;
    params.clock_tx_timeout    = 5;
    params.smeter_needle.x     = true;   /* the needle S-meter (repo feature) */
    params.mag_freq.x          = false;
    params.mag_info.x          = false;
    params.freq_accel.x        = FREQ_ACCEL_NONE;
    snprintf(params.theme_name.x, sizeof(params.theme_name.x), "%s", "IU1WXY");
}

void params_init(void)
{
    mock_params_init();
}

void params_lock(void)
{
    pthread_mutex_lock(&params_mux);
}

void params_unlock(bool *dirty)
{
    if (dirty) {
        *dirty = true;
    }
    pthread_mutex_unlock(&params_mux);
}

bool params_ready_to_save(void)
{
    return false;
}

void params_flush(void) {}

void params_bool_set(params_bool_t *var, bool x)
{
    var->x = x;
    var->dirty = true;
}

void params_uint8_set(params_uint8_t *var, uint8_t x)
{
    var->x = x;
    var->dirty = true;
}

void params_uint16_set(params_uint16_t *var, uint16_t x)
{
    var->x = x;
    var->dirty = true;
}

void params_int32_set(params_int32_t *var, int32_t x)
{
    var->x = x;
    var->dirty = true;
}

void params_float_set(params_float_t *var, float x)
{
    var->x = x;
    var->dirty = true;
}

void params_str_set(params_str_t *var, const char *x)
{
    snprintf(var->x, sizeof(var->x), "%s", x);
    var->dirty = true;
}

uint8_t params_uint8_change(params_uint8_t *var, int16_t df)
{
    int32_t v = (int32_t) var->x + df;

    if (v < var->min) v = var->min;
    if (v > var->max) v = var->max;
    var->x = (uint8_t) v;
    var->dirty = true;
    return var->x;
}

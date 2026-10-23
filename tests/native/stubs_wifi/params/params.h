/*
 * Minimal params/params.h for the WiFi idle-pump native stage (issue #218, F-06).
 * Only the field wifi.cpp touches (`params.wifi_enabled.x`) and the setter it calls, with
 * the same shape as the real header/boot (src/params/types.h y src/params/params.c): el
 * `.x` es el valor vigente y `params_bool_set()` lo escribe y marca dirty.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *name;
    char *voice;
    bool  x;
    bool  dirty;
} params_bool_t;

/* El `struct params` de la ROM, recortado al unico campo que este modulo usa. */
extern struct wifi_fx_params {
    params_bool_t wifi_enabled;
} params;

void params_bool_set(params_bool_t *var, bool x);

#ifdef __cplusplus
}
#endif

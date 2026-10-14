/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - politica visual de bateria (UI-08 incr. 1 y 2).
 *
 *  Implementacion de visual_policy.h. La evaluacion es pura y sin
 *  dependencias; el test nativo (tests/native/test_visual_policy.c) la cubre
 *  entera. La unica salida al mundo es
 *  visual_policy_waterfall_apply() (incr. 2), que aplica el perfil al modo
 *  Ahorro del waterfall: por eso este archivo incluye waterfall.h, y el test
 *  nativo compila contra el stub que registra esa llamada.
 */

#include "visual_policy.h"

#include <string.h>

#include "waterfall.h"

/* Umbral de entrada (bajando) y de salida (subiendo) por banda. QUALITY, la
 * mejor, no tiene umbral propio: es el resto, por eso va en VP_NO_THRESHOLD. */
typedef struct {
    uint16_t enter_mv;
    uint16_t exit_mv;
} vp_band_t;

static const vp_band_t VP_BANDS[VP_PROFILE_COUNT] = {
    [VP_QUALITY]  = { VP_NO_THRESHOLD,       VP_NO_THRESHOLD },
    [VP_BALANCED] = { VP_BALANCED_ENTER_MV,  VP_BALANCED_EXIT_MV },
    [VP_ECO]      = { VP_ECO_ENTER_MV,       VP_ECO_EXIT_MV },
    [VP_CRITICAL] = { VP_CRITICAL_ENTER_MV,  VP_CRITICAL_EXIT_MV },
};

/* La muestra se descarta entera si el voltaje esta fuera de rango, la
 * capacidad no es un porcentaje, o la bandera de carga es imposible (cargando
 * sin fuente externa). */
static bool vp_sample_valid(uint16_t vbat_mv, uint16_t vext_mv,
                            uint8_t batcap, bool charging) {
    if (vbat_mv < VP_VBAT_MIN_MV || vbat_mv > VP_VBAT_MAX_MV) {
        return false;
    }

    if (batcap > VP_BATCAP_MAX_PCT) {
        return false;
    }

    if (charging && vext_mv < VP_VEXT_PRESENT_MV) {
        return false;
    }

    return true;
}

/* Banda a la que apunta el voltaje, con hysteresis respecto del perfil actual:
 * bajar de banda solo al cruzar el umbral de ENTRADA de la banda siguiente;
 * subir solo al superar el umbral de SALIDA de la banda actual, que es mas
 * exigente. Recorrer de a un escalon evita los saltos y el flap. */
static visual_profile_t vp_auto_profile(uint16_t vbat_mv, visual_profile_t current) {
    int profile = (int) current;

    while (profile < (int) VP_CRITICAL &&
           vbat_mv <= VP_BANDS[profile + 1].enter_mv) {
        profile++;
    }

    while (profile > (int) VP_QUALITY &&
           vbat_mv >= VP_BANDS[profile].exit_mv) {
        profile--;
    }

    return (visual_profile_t) profile;
}

void visual_policy_init(visual_policy_t *state) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->profile = VP_QUALITY;
    state->output = VP_QUALITY;
    state->manual_profile = VP_QUALITY;
}

visual_policy_result_t visual_policy_eval(visual_policy_t *state,
                                          uint16_t vbat_mv, uint16_t vext_mv,
                                          uint8_t batcap, bool charging,
                                          uint32_t last_valid_ms, uint32_t now_ms,
                                          visual_override_t override) {
    visual_policy_result_t result = {0};

    if (!state) {
        return result;
    }

    uint32_t age_ms = (uint32_t) (now_ms - last_valid_ms);
    bool     external = (vext_mv >= VP_VEXT_PRESENT_MV) || charging;

    result.previous = state->output;
    result.age_ms = age_ms;
    result.stale = age_ms > VP_STALE_MS;

    /* Override manual: fija el perfil y congela el AUTO (la memoria de
     * hysteresis no se toca). La staleness se reporta pero no manda: el
     * usuario eligio el perfil. */
    if (override.manual) {
        state->manual = true;
        state->manual_profile = override.profile;
        state->stale = result.stale;
        result.profile = override.profile;
        result.manual = true;
        result.changed = (result.profile != result.previous);
        state->output = result.profile;
        return result;
    }

    state->manual = false;

    /* Lectura invalida: se descarta la muestra entera; la politica y el estado
     * quedan intactos (sin transicion). */
    if (!vp_sample_valid(vbat_mv, vext_mv, batcap, charging)) {
        result.profile = state->output;
        result.discarded = true;
        result.changed = false;
        return result;
    }

    visual_profile_t profile;

    if (external) {
        /* Fuente externa: calidad plena de inmediato, sin hysteresis. */
        profile = VP_QUALITY;
    } else if (result.stale) {
        /* Datos viejos: fallback conservador declarado. */
        profile = VP_STALE_FALLBACK;
    } else {
        profile = vp_auto_profile(vbat_mv, state->profile);
    }

    state->stale = result.stale;
    state->profile = profile;
    state->output = profile;

    result.profile = profile;
    result.changed = (profile != result.previous);

    return result;
}

void visual_policy_waterfall_apply(visual_profile_t profile, bool user_battery_mode) {
    /* Los perfiles que ahorran trabajo en la cascada son ECO y CRITICAL: el
     * perfil manda y el toggle del usuario queda intacto (el param no se toca).
     * QUALITY y BALANCED no fuerzan nada: vale el toggle del usuario. */
    bool saving = (profile == VP_ECO || profile == VP_CRITICAL);

    waterfall_set_battery_mode(saving || user_battery_mode);
}

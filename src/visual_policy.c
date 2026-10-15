/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - politica visual de bateria (UI-08 incr. 1 a 5).
 *
 *  Implementacion de visual_policy.h. La evaluacion es pura y sin
 *  dependencias; el test nativo (tests/native/test_visual_policy.c) la cubre
 *  entera. Las salidas al mundo son
 *  visual_policy_waterfall_apply() (incr. 2), que aplica el perfil al modo
 *  Ahorro del waterfall, visual_policy_spectrum_apply() (cadencia ECO del
 *  espectro, corrida eco-final), y visual_policy_brightness_apply() /
 *  visual_policy_brightness_rewrite() (incr. 3 y 4), que lo aplican al nivel
 *  del backlight: por eso este archivo incluye waterfall.h, spectrum.h y
 *  backlight_level.h, y el test nativo compila contra los stubs que registran
 *  esas llamadas.
 */

#include "visual_policy.h"

#include <string.h>

#include "backlight_level.h"
#include "spectrum.h"
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

/* Cap de brillo por perfil (nivel maximo permitido sobre el setting del
 * usuario). Es la tabla del contrato del incr. 3: QUALITY y BALANCED no
 * recortan (cap = maximo del panel) y los que ahorran si. La tabla y los
 * #define son el unico lugar con los valores; el test nativo los fija con
 * literales, asi que mover un cap deja el test rojo. */
static const int16_t VP_BRIGHTNESS_CAP[VP_PROFILE_COUNT] = {
    [VP_QUALITY]  = VP_BRIGHTNESS_QUALITY,
    [VP_BALANCED] = VP_BRIGHTNESS_BALANCED,
    [VP_ECO]      = VP_BRIGHTNESS_ECO,
    [VP_CRITICAL] = VP_BRIGHTNESS_CRITICAL,
};

/* El cap no puede salirse del dominio del panel (el slider de Ajustes), y solo
 * puede bajar: si creciera al ahorrar mas, un perfil mas austero seria mas
 * brillante que uno mejor. */
_Static_assert(VP_BRIGHTNESS_CRITICAL >= 0 &&
               VP_BRIGHTNESS_CRITICAL < VP_BRIGHTNESS_ECO &&
               VP_BRIGHTNESS_ECO < VP_BRIGHTNESS_MAX,
               "ECO y CRITICAL tienen que recortar de verdad, en ese orden");
_Static_assert(VP_BRIGHTNESS_QUALITY == VP_BRIGHTNESS_MAX &&
               VP_BRIGHTNESS_BALANCED == VP_BRIGHTNESS_MAX,
               "QUALITY y BALANCED no recortan: cap = maximo del panel");

/* Cap VIGENTE: el del ultimo perfil aplicado. Es el unico estado propio del
 * modulo y existe por el enforcement del incremento 4: el tick por input y el
 * timer de idle (src/backlight.c) reescriben el nivel desde params sin conocer
 * el perfil, asi que el recorte tiene que sobrevivir entre transiciones. Lo
 * fija visual_policy_brightness_apply() (la unica que ve el perfil) y lo lee
 * visual_policy_brightness_rewrite(). Arranca en el maximo del panel: antes de
 * cualquier apply no hay recorte, que es el perfil inicial (QUALITY). Un solo
 * hilo lo toca: en la radio el apply de radio.c y los ticks de input/timer
 * corren en el hilo de LVGL, y el arnes corre en un unico hilo. */
static int16_t vp_brightness_cap = VP_BRIGHTNESS_MAX;

/* Perfil VIGENTE: el ultimo que paso por visual_policy_brightness_apply() (el
 * apply que ya fija el cap y llega al panel). Existe para la UI de Ajustes
 * (visual_policy_current), que tiene que mostrar el estado real y no solo el
 * seteado. Se escribe en el APPLY y no en la evaluacion a proposito: en la radio
 * la evaluacion corre en el hilo de la radio (radio_tick, src/radio.c) y el
 * apply en el de LVGL (lo encola el scheduler), que es el mismo hilo del
 * dialogo; escribir aca deja la lectura del dialogo en un solo hilo. Arranca en
 * QUALITY: antes del primer apply el perfil efectivo es ese. */
static visual_profile_t vp_current = VP_QUALITY;

/* Cap declarado de un perfil: UNICA lectura de la tabla. Un perfil fuera del
 * enum no puede indexarla y se trata como el perfil sin recorte (el mismo que
 * la politica usa por defecto). La usan el apply (lo fija como vigente) y el
 * rewrite (lo lee); por eso el enforcement no puede divergir del recorte. */
static int16_t vp_brightness_cap_of(visual_profile_t profile) {
    int index = (int) profile;

    if (index < 0 || index >= (int) VP_PROFILE_COUNT) {
        index = (int) VP_QUALITY;
    }

    return VP_BRIGHTNESS_CAP[index];
}

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

visual_override_t visual_policy_override_from_param(uint8_t value) {
    /* 0..VP_PROFILE_COUNT-1 es un perfil fijado; todo lo demas (incluido
     * VP_PARAM_AUTO y cualquier byte desconocido) es el modo automatico. */
    if (value < VP_PROFILE_COUNT) {
        return vp_override_manual((visual_profile_t) value);
    }

    return vp_override_auto();
}

const char *visual_policy_profile_name(visual_profile_t profile) {
    switch (profile) {
        case VP_QUALITY:
            return "Quality";
        case VP_BALANCED:
            return "Balanced";
        case VP_ECO:
            return "Eco";
        case VP_CRITICAL:
            return "Critical";
        default:
            return "Quality";
    }
}

visual_profile_t visual_policy_current(void) {
    return vp_current;
}

void visual_policy_waterfall_apply(visual_profile_t profile, bool user_battery_mode) {
    /* Los perfiles que ahorran trabajo en la cascada son ECO y CRITICAL: el
     * perfil manda y el toggle del usuario queda intacto (el param no se toca).
     * QUALITY y BALANCED no fuerzan nada: vale el toggle del usuario. */
    bool saving = (profile == VP_ECO || profile == VP_CRITICAL);

    waterfall_set_battery_mode(saving || user_battery_mode);
}

void visual_policy_spectrum_apply(visual_profile_t profile) {
    /* Mismo reparto que el waterfall: ECO y CRITICAL son los perfiles que ahorran
     * trabajo de render; QUALITY y BALANCED vuelven a dibujar todos los refrescos.
     * El espectro no tiene toggle del usuario, asi que el perfil es la entrada
     * entera. */
    bool saving = (profile == VP_ECO || profile == VP_CRITICAL);

    spectrum_set_refresh_period(saving ? VP_SPECTRUM_ECO_PERIOD : 1);
}

void visual_policy_brightness_apply(visual_profile_t profile, int16_t user_setting) {
    /* El perfil fija el cap vigente (el que veran los rewrites del tick/timer) y
     * el nivel sale del MISMO clamp que esos rewrites: el recorte del apply y el
     * del rewrite no pueden diferir. Deja tambien el perfil VIGENTE que consulta
     * la UI (visual_policy_current): el apply corre en el hilo de LVGL, el mismo
     * del dialogo, asi que la lectura no cruza hilos. */
    vp_brightness_cap = vp_brightness_cap_of(profile);
    vp_current = profile;

    visual_policy_brightness_rewrite(user_setting);
}

void visual_policy_brightness_rewrite(int16_t user_setting) {
    /* El cap solo BAJA el nivel: el setting del usuario es el techo y nunca se
     * escribe (runtime, el disco no se toca). Un nivel negativo (el usuario
     * apago el display) queda negativo: el perfil no lo revive. */
    int16_t cap = vp_brightness_cap;

    int16_t level = (user_setting < cap) ? user_setting : cap;

    backlight_set_brightness(level);
}

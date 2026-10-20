/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Adaptive Tuning (issue #186)
 *
 *  Curva aprobada (seccion 2): velocidad real en detents/s, niveles discretos,
 *  umbrales derivados de T y multiplicadores FIJOS. Retirados expresamente los
 *  submultiplicadores de 0,1x y el tope de 100x del motor anterior.
 *
 *  Copyright (c) 2024 LU2DSX
 */

#include "adaptive_tuning.h"

#include <stddef.h>
#include <string.h>

/* Tabla fija de multiplicadores: [perfil][nivel]. NONE no acelera nunca. */
static const uint8_t multipliers[3][ADAPTIVE_LEVELS + 1] = {
    [ADAPTIVE_ACCEL_NONE]   = { 1, 1, 1, 1, 1 },
    [ADAPTIVE_ACCEL_LITE]   = { 1, 2, 3, 6, 10 },
    [ADAPTIVE_ACCEL_STRONG] = { 1, 3, 6, 12, 24 },
};

void adaptive_tuning_reset(adaptive_tuning_state_t *state) {
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->last_direction = 0;
    state->level          = 0;
}

void adaptive_tuning_init(adaptive_tuning_state_t *state) {
    adaptive_tuning_reset(state);
}

uint8_t adaptive_tuning_threshold_sanitize(uint8_t threshold_dps) {
    if (threshold_dps < ADAPTIVE_THRESHOLD_MIN || threshold_dps > ADAPTIVE_THRESHOLD_MAX) {
        return ADAPTIVE_THRESHOLD_DEFAULT;
    }

    return threshold_dps;
}

void adaptive_tuning_thresholds(uint8_t threshold_dps, uint8_t out[ADAPTIVE_LEVELS]) {
    uint16_t t = adaptive_tuning_threshold_sanitize(threshold_dps);

    if (out == NULL) {
        return;
    }

    /* ceil(n * T / 7) con aritmetica entera: (n*T + 6) / 7. */
    out[0] = (uint8_t) t;
    out[1] = (uint8_t) ((15 * t + 6) / 7);
    out[2] = (uint8_t) ((30 * t + 6) / 7);
    out[3] = (uint8_t) ((50 * t + 6) / 7);
}

uint8_t adaptive_tuning_multiplier(adaptive_accel_t profile, uint8_t level) {
    if (level > ADAPTIVE_LEVELS) {
        level = ADAPTIVE_LEVELS;
    }

    if (profile != ADAPTIVE_ACCEL_LITE && profile != ADAPTIVE_ACCEL_STRONG) {
        profile = ADAPTIVE_ACCEL_NONE;
    }

    return multipliers[profile][level];
}

uint8_t adaptive_tuning_detent(adaptive_tuning_state_t *state, int direction,
                               uint64_t timestamp_us, adaptive_accel_t profile,
                               uint8_t threshold_dps) {
    if (state == NULL) {
        return 1;
    }

    /* None = siempre x1 y sin aceleracion retenida. */
    if (profile != ADAPTIVE_ACCEL_LITE && profile != ADAPTIVE_ACCEL_STRONG) {
        adaptive_tuning_reset(state);
        return 1;
    }

    if (direction > 0) {
        direction = 1;
    } else if (direction < 0) {
        direction = -1;
    } else {
        /* No es un detent: no toca el estado. */
        return 1;
    }

    threshold_dps = adaptive_tuning_threshold_sanitize(threshold_dps);

    /* Primer detent (o primero tras un reset): arranca en nivel 0, sin inventar
     * velocidad. */
    if (!state->have_timestamp) {
        state->have_timestamp       = true;
        state->last_timestamp_us    = timestamp_us;
        state->last_direction       = (int8_t) direction;
        state->smoothed_dps         = 0.0f;
        state->smoothed_valid       = false;
        state->level                = 0;
        state->warmup               = 0;
        return 1;
    }

    uint64_t ts      = timestamp_us;
    bool     anomaly = false;

    /* Marca no creciente: separacion virtual minima de 1000 us y ese detent no
     * puede acelerar por si solo (seccion 2.1). */
    if (ts <= state->last_timestamp_us) {
        ts      = state->last_timestamp_us + ADAPTIVE_MIN_DT_US;
        anomaly = true;
    }

    uint64_t dt_us = ts - state->last_timestamp_us;

    state->last_timestamp_us = ts;

    /* Inversion de direccion: el primer detent contrario es x1 y descarta la
     * velocidad anterior. */
    if (state->last_direction != (int8_t) direction) {
        state->last_direction = (int8_t) direction;
        state->smoothed_valid = false;
        state->smoothed_dps   = 0.0f;
        state->level          = 0;
        state->warmup         = 0;
        return 1;
    }

    if (anomaly) {
        state->smoothed_valid = false;
        state->smoothed_dps   = 0.0f;
        state->level          = 0;
        state->warmup         = 0;
        return 1;
    }

    /* Freno: a partir de 180 ms el siguiente detent vuelve a precision y pierde
     * la aceleracion (sin inercia: la frecuencia solo cambia con detents). */
    if (dt_us >= ADAPTIVE_PAUSE_US) {
        state->smoothed_valid = false;
        state->smoothed_dps   = 0.0f;
        state->level          = 0;
        state->warmup         = 0;
        return 1;
    }

    if (dt_us < ADAPTIVE_MIN_DT_US) {
        dt_us = ADAPTIVE_MIN_DT_US;
    }

    /* Velocidad instantanea en detents/s (el float es solo para el filtro: el
     * resultado que sale del motor es siempre entero). */
    float v = 1000000.0f / (float) dt_us;

    if (v > ADAPTIVE_MAX_SPEED_DPS) {
        v = ADAPTIVE_MAX_SPEED_DPS;
    }

    /* Filtro: subida amortiguada, bajada inmediata (desacelerar sin cola). */
    if (!state->smoothed_valid) {
        state->smoothed_dps   = v;
        state->smoothed_valid = true;
    } else if (v < state->smoothed_dps) {
        state->smoothed_dps = v;
    } else {
        state->smoothed_dps = ADAPTIVE_SMOOTH_UP * v + (1.0f - ADAPTIVE_SMOOTH_UP) * state->smoothed_dps;
    }

    if (state->warmup < 0xFF) {
        state->warmup++;
    }

    uint8_t thresholds[ADAPTIVE_LEVELS];

    adaptive_tuning_thresholds(threshold_dps, thresholds);

    /* Subidas: entrar al nivel L+1 exige smoothed >= T(L+1). */
    while (state->level < ADAPTIVE_LEVELS &&
           state->smoothed_dps >= (float) thresholds[state->level]) {
        state->level++;
    }

    /* Bajadas: salir del nivel L cuando smoothed < 0.80 x T(L). */
    while (state->level > 0 &&
           state->smoothed_dps < ADAPTIVE_HYSTERESIS * (float) thresholds[state->level - 1]) {
        state->level--;
    }

    /* Arranque: en los dos primeros intervalos validos no se pasa de nivel 1. */
    if (state->warmup <= ADAPTIVE_WARMUP_INTERVALS && state->level > 1) {
        state->level = 1;
    }

    return adaptive_tuning_multiplier(profile, state->level);
}

int64_t adaptive_tuning_weighted_event(adaptive_tuning_state_t *state, int32_t diff,
                                       uint64_t timestamp_us, adaptive_accel_t profile,
                                       uint8_t threshold_dps, uint8_t *last_multiplier) {
    if (state == NULL || diff == 0) {
        return 0;
    }

    int      direction = diff > 0 ? 1 : -1;
    int64_t  count     = diff > 0 ? (int64_t) diff : -(int64_t) diff;
    int64_t  sum       = 0;

    /* Base de la interpolacion: el timestamp del EVENTO anterior. Despues de
     * procesar un evento completo, el ultimo detent cae exactamente en el
     * timestamp de ese evento, asi que last_timestamp_us sirve de base. Si no
     * hay marca anterior (primer evento), todos los detents van al mismo tiempo
     * y el motor los da x1. */
    uint64_t base = state->have_timestamp ? state->last_timestamp_us : timestamp_us;

    if (timestamp_us <= base) {
        base = timestamp_us;        /* marca repetida o no creciente: anomalia */
    }

    uint64_t span = timestamp_us - base;

    for (int64_t k = 1; k <= count; k++) {
        uint64_t ts;

        if (span > 0 && count > 1) {
            /* Reparto uniforme sin desbordar: (span/n)*k + ((span%n)*k)/n */
            uint64_t n = (uint64_t) count;

            ts = base + (span / n) * (uint64_t) k + ((span % n) * (uint64_t) k) / n;
        } else {
            ts = timestamp_us;
        }

        uint8_t multiplier = adaptive_tuning_detent(state, direction, ts, profile, threshold_dps);

        sum += (int64_t) direction * (int64_t) multiplier;

        if (last_multiplier != NULL) {
            *last_multiplier = multiplier;
        }
    }

    return sum;
}

/* El payload lleva `raw_diff` en int32: se recorta solo en el borde (una cola
 * de 512 entradas de EV_REL reales nunca se acerca, pero el contrato prohibe
 * desbordar en silencio). */
static int32_t clamp_raw(int64_t raw) {
    if (raw > INT32_MAX) {
        return INT32_MAX;
    }

    if (raw < INT32_MIN) {
        return INT32_MIN;
    }

    return (int32_t) raw;
}

size_t adaptive_tuning_segments_ex(adaptive_tuning_state_t *state,
                                   const int32_t *diffs, const uint64_t *timestamps_us, size_t n,
                                   adaptive_accel_t profile, uint8_t threshold_dps,
                                   rotary_event_enhanced_t *out, size_t out_max,
                                   uint8_t *event_multipliers, size_t event_multipliers_max) {
    size_t   written   = 0;
    int64_t  raw       = 0;
    int64_t  weighted  = 0;
    uint8_t  last_mult = 1;

    if (state == NULL || diffs == NULL || timestamps_us == NULL || out == NULL) {
        return 0;
    }

    for (size_t i = 0; i < n; i++) {
        if (event_multipliers != NULL && i < event_multipliers_max) {
            event_multipliers[i] = 0;
        }

        /* diff == 0: discontinuidad del reloj (ver src/rotary.c). Resetea y
         * corta el tramo: no se netean signos ni se inventa un salto. */
        if (diffs[i] == 0) {
            adaptive_tuning_reset(state);

            if (raw != 0 && written < out_max) {
                out[written].raw_diff         = clamp_raw(raw);
                out[written].weighted_detents = weighted;
                out[written].last_multiplier  = last_mult;
                written++;
            }

            raw       = 0;
            weighted  = 0;
            last_mult = 1;
            continue;
        }

        /* Cambio de signo: se emite el tramo anterior EN ORDEN, nunca se fusiona
         * en un neto (una reversa tiene que llegar como dos tramos). */
        if (raw != 0 && ((diffs[i] > 0) != (raw > 0))) {
            if (written < out_max) {
                out[written].raw_diff         = clamp_raw(raw);
                out[written].weighted_detents = weighted;
                out[written].last_multiplier  = last_mult;
                written++;
            }

            raw      = 0;
            weighted = 0;
        }

        uint8_t mult = 1;

        weighted += adaptive_tuning_weighted_event(state, diffs[i], timestamps_us[i],
                                                   profile, threshold_dps, &mult);
        raw      += diffs[i];
        last_mult = mult;

        if (event_multipliers != NULL && i < event_multipliers_max) {
            event_multipliers[i] = mult;
        }
    }

    if (raw != 0 && written < out_max) {
        out[written].raw_diff         = clamp_raw(raw);
        out[written].weighted_detents = weighted;
        out[written].last_multiplier  = last_mult;
        written++;
    }

    return written;
}

size_t adaptive_tuning_segments(adaptive_tuning_state_t *state,
                                const int32_t *diffs, const uint64_t *timestamps_us, size_t n,
                                adaptive_accel_t profile, uint8_t threshold_dps,
                                rotary_event_enhanced_t *out, size_t out_max) {
    return adaptive_tuning_segments_ex(state, diffs, timestamps_us, n, profile, threshold_dps,
                                       out, out_max, NULL, 0);
}

bool adaptive_tuning_target_freq(int32_t current, int64_t weighted_detents,
                                 int32_t step, int32_t *out) {
    int64_t target;

    if (out == NULL) {
        return false;
    }

    /* Toda la aritmetica en int64: nunca desborda por weighted * step. */
    target = (int64_t) current + weighted_detents * (int64_t) step;

    if (target < INT32_MIN || target > INT32_MAX) {
        return false;
    }

    *out = (int32_t) target;

    return true;
}

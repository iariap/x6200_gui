/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Fine-tuning assistant (autotune) implementation
 *
 *  Copyright (c) 2024 LU2DSX
 *
 *  Auto Tune v2 (issue #190): the policy in one place, no UI. See src/autotune.h
 *  for the contract and the numeric constants; docs/autotune.md for the whole
 *  function.
 */

#include "autotune.h"

#include <math.h>
#include <stddef.h>

/* --- estadistica robusta --------------------------------------------------- */

float autotune_median(const float *values, int n) {
    float buf[AUTOTUNE_PHASE_SAMPLES];
    int   i, j;

    if (values == NULL || n <= 0) {
        return 0.0f;
    }
    if (n > AUTOTUNE_PHASE_SAMPLES) {
        n = AUTOTUNE_PHASE_SAMPLES;
    }

    /* Copia local (n es chico y fijo), insertion sort y el del medio: un pico
     * aislado no mueve la mediana. La copia es lo que permite leer buf[j - 1]
     * sabiendo que todo lo que esta debajo del cursor ya esta ordenado. */
    for (i = 0; i < n; i++) {
        buf[i] = values[i];
    }
    for (i = 1; i < n; i++) {
        float v = buf[i];

        for (j = i; j > 0 && buf[j - 1] > v; j--) {
            buf[j] = buf[j - 1];
        }
        buf[j] = v;
    }

    if (n % 2 == 1) {
        return buf[n / 2];
    }
    return (buf[n / 2 - 1] + buf[n / 2]) / 2.0f;
}

bool autotune_stable(const float *values, int n) {
    float min;
    float max;

    if (values == NULL || n <= 0) {
        return false;
    }

    min = values[0];
    max = values[0];
    for (int i = 1; i < n; i++) {
        if (values[i] < min) {
            min = values[i];
        }
        if (values[i] > max) {
            max = values[i];
        }
    }
    return (max - min) <= AUTOTUNE_MAX_SPREAD_HZ;
}

bool autotune_sample_ok(const autotune_sample_t *s) {
    if (s == NULL || !s->valid) {
        return false;
    }
    if (!isfinite(s->error_hz) || !isfinite(s->ratio)) {
        return false;
    }
    if (s->ratio < AUTOTUNE_MIN_SNR_RATIO) {
        return false;
    }
    if (s->error_hz < -AUTOTUNE_SAMPLE_ERROR_LIMIT_HZ ||
        s->error_hz > AUTOTUNE_SAMPLE_ERROR_LIMIT_HZ) {
        return false;
    }
    return true;
}

/* --- politica de la correccion --------------------------------------------- */

int32_t autotune_dial_delta(float median_hz, autotune_mode_t mode) {
    int32_t delta;

    if (!isfinite(median_hz)) {
        return 0;
    }
    /* roundf() redondea al entero mas cercano alejandose del cero: el signo del
     * error se conserva y no depende del redondeo del compilador. */
    delta = (int32_t) roundf(median_hz);
    if (mode == AUTOTUNE_MODE_CWR) {
        delta = -delta;
    }
    return delta;
}

bool autotune_target_in_band(int32_t target, uint32_t band_start, uint32_t band_stop) {
    if (target < 0 || band_stop < band_start) {
        return false;
    }
    return (uint32_t) target >= band_start && (uint32_t) target <= band_stop;
}

/* --- propiedad del dial (issue #190, seccion 5) ---------------------------- */

bool autotune_context_identity_equal(const autotune_context_t *a, const autotune_context_t *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->mode == b->mode && a->band_id == b->band_id && a->vfo == b->vfo;
}

bool autotune_context_broken(const autotune_t *at, const autotune_context_t *now) {
    if (at == NULL || now == NULL) {
        return true;
    }
    if (at->state == AUTOTUNE_STATE_IDLE) {
        return true;
    }
    if (!now->rx || now->locked || now->rit) {
        return true;
    }
    if (now->pitch != at->snap.pitch) {
        return true;
    }
    return !autotune_context_identity_equal(&at->snap, now);
}

bool autotune_owns(const autotune_t *at, const autotune_context_t *now) {
    if (autotune_context_broken(at, now)) {
        return false;
    }
    /* El dial tiene que seguir donde lo dejo Auto Tune: si alguien lo movio
     * (rueda, CAT, memoria, app, usuario), no se escribe nunca encima. */
    return now->freq == at->expected_freq;
}

/* --- sesion ---------------------------------------------------------------- */

static void phase_clear(autotune_t *at, uint64_t now_ms, uint64_t last_seq) {
    at->n = 0;
    at->dds_offset = 0.0f;
    at->phase_start_ms = now_ms;
    at->min_ms = now_ms;
    at->last_seq = last_seq;
    at->last_ts = 0;
    at->have_last = false;
    for (int i = 0; i < AUTOTUNE_PHASE_SAMPLES; i++) {
        at->err[i] = 0.0f;
    }
}

static void context_zero(autotune_context_t *ctx) {
    ctx->mode = 0;
    ctx->band_id = 0;
    ctx->vfo = 0;
    ctx->pitch = 0;
    ctx->freq = 0;
    ctx->rx = false;
    ctx->locked = false;
    ctx->rit = false;
}

void autotune_reset(autotune_t *at) {
    if (at == NULL) {
        return;
    }
    at->state = AUTOTUNE_STATE_IDLE;
    at->mode = AUTOTUNE_MODE_CW;
    at->start_freq = 0;
    at->expected_freq = 0;
    at->wrote = false;
    at->delta_hz = 0;
    at->session_start_ms = 0;
    context_zero(&at->snap);
    phase_clear(at, 0, 0);
}

void autotune_collect_start(autotune_t *at, autotune_mode_t mode,
                            const autotune_context_t *ctx, uint64_t now_ms,
                            uint64_t last_seq) {
    if (at == NULL || ctx == NULL) {
        return;
    }
    autotune_reset(at);
    at->state = AUTOTUNE_STATE_COLLECT;
    at->mode = mode;
    at->snap = *ctx;
    at->start_freq = ctx->freq;
    at->expected_freq = ctx->freq;
    at->session_start_ms = now_ms;
    phase_clear(at, now_ms, last_seq);
}

void autotune_note_write(autotune_t *at, int32_t target) {
    if (at == NULL) {
        return;
    }
    at->wrote = true;
    at->expected_freq = target;
}

void autotune_note_rollback(autotune_t *at) {
    if (at == NULL) {
        return;
    }
    at->wrote = false;
    at->expected_freq = at->start_freq;
}

void autotune_verify_start(autotune_t *at, uint64_t now_ms, uint64_t last_seq,
                           uint64_t write_ms) {
    if (at == NULL) {
        return;
    }
    at->state = AUTOTUNE_STATE_VERIFY;
    phase_clear(at, now_ms, last_seq);
    /* La guarda se mide desde la ESCRITURA, no desde este instante. */
    at->min_ms = write_ms + AUTOTUNE_SETTLE_GUARD_MS;
}

autotune_take_t autotune_take(autotune_t *at, const autotune_sample_t *s, uint64_t now_ms) {
    (void) now_ms;

    if (at == NULL || s == NULL || at->state == AUTOTUNE_STATE_IDLE) {
        return AUTOTUNE_TAKE_IGNORED;
    }
    if (!autotune_sample_ok(s)) {
        return AUTOTUNE_TAKE_IGNORED;
    }
    /* Frescura real: secuencia estrictamente creciente y timestamp estrictamente
     * creciente, ademas del piso de tiempo de la fase (post-escritura + guarda
     * en VERIFY). Una trama vieja, repetida, duplicada o desordenada no entra:
     * que venza el timer no prueba frescura. */
    if (s->sequence <= at->last_seq || s->timestamp_ms < at->min_ms ||
        (at->have_last && s->timestamp_ms <= at->last_ts)) {
        return AUTOTUNE_TAKE_IGNORED;
    }

    /* Cambio el centro del DDS (AUTO #139 movio `rx_tone_offset`): se vacian las
     * medidas acumuladas y se empieza la fase de nuevo. Nunca se mezclan
     * muestras de dos centros. */
    if (at->n > 0 && s->dds_offset_hz != at->dds_offset) {
        at->n = 0;
    }
    at->dds_offset = s->dds_offset_hz;
    at->last_seq = s->sequence;
    at->last_ts = s->timestamp_ms;
    at->have_last = true;

    if (at->n >= AUTOTUNE_PHASE_SAMPLES) {
        /* Ventana corrediza: la trama mas vieja sale (una atipica se va sola). */
        for (int i = 1; i < AUTOTUNE_PHASE_SAMPLES; i++) {
            at->err[i - 1] = at->err[i];
        }
        at->n--;
    }
    at->err[at->n++] = s->error_hz;

    if (at->n < AUTOTUNE_PHASE_SAMPLES) {
        return AUTOTUNE_TAKE_MORE;
    }
    if (!autotune_stable(at->err, at->n)) {
        return AUTOTUNE_TAKE_MORE;
    }
    return AUTOTUNE_TAKE_READY;
}

bool autotune_phase_ready(const autotune_t *at) {
    return at != NULL && at->n >= AUTOTUNE_PHASE_SAMPLES && autotune_stable(at->err, at->n);
}

bool autotune_phase_timeout(const autotune_t *at, uint64_t now_ms) {
    if (at == NULL || at->state == AUTOTUNE_STATE_IDLE) {
        return false;
    }
    return (now_ms - at->phase_start_ms) > AUTOTUNE_PHASE_TIMEOUT_MS;
}

float autotune_phase_median(const autotune_t *at) {
    if (at == NULL || at->n < AUTOTUNE_PHASE_SAMPLES) {
        return 0.0f;
    }
    return autotune_median(at->err, at->n);
}

bool autotune_session_timeout(const autotune_t *at, uint64_t now_ms) {
    if (at == NULL || at->state == AUTOTUNE_STATE_IDLE) {
        return false;
    }
    return (now_ms - at->session_start_ms) > AUTOTUNE_SESSION_TIMEOUT_MS;
}

autotune_result_t autotune_collect_result(const autotune_t *at, uint32_t band_start,
                                          uint32_t band_stop, int32_t *target) {
    float   median;
    int32_t target_freq;

    if (at == NULL || at->state != AUTOTUNE_STATE_COLLECT || target == NULL) {
        return AUTOTUNE_RESULT_NO_ADJUST;
    }
    if (!autotune_phase_ready(at)) {
        return AUTOTUNE_RESULT_NO_ADJUST;
    }

    median = autotune_median(at->err, at->n);
    if (!isfinite(median)) {
        return AUTOTUNE_RESULT_NO_ADJUST;
    }
    if (fabsf(median) <= AUTOTUNE_DEADBAND_HZ) {
        return AUTOTUNE_RESULT_TUNED;       /* ya estaba bien: cero escrituras */
    }
    /* Por encima del tope NO se recorta una correccion de 500 Hz a 300: no se
     * toca el dial. */
    if (fabsf(median) > AUTOTUNE_MAX_DELTA_HZ) {
        return AUTOTUNE_RESULT_NO_ADJUST;
    }

    target_freq = (int32_t) ((int64_t) at->start_freq + autotune_dial_delta(median, at->mode));

    if (target_freq == at->start_freq) {
        return AUTOTUNE_RESULT_NO_ADJUST;   /* el redondeo dio 0: sin escritura */
    }
    if (!autotune_target_in_band(target_freq, band_start, band_stop)) {
        return AUTOTUNE_RESULT_NO_ADJUST;
    }

    *target = target_freq;
    return AUTOTUNE_RESULT_CORRECT;
}

autotune_result_t autotune_verify_result(const autotune_t *at) {
    if (at == NULL || at->state != AUTOTUNE_STATE_VERIFY || !autotune_phase_ready(at)) {
        return AUTOTUNE_RESULT_NO_ADJUST;
    }
    if (fabsf(autotune_median(at->err, at->n)) <= AUTOTUNE_DEADBAND_HZ) {
        return AUTOTUNE_RESULT_TUNED;
    }
    return AUTOTUNE_RESULT_NO_ADJUST;
}

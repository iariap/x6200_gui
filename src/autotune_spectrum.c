/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Auto Tune multimode spectrum policies (issue #205)
 *
 *  Copyright (c) 2024 LU2DSX
 *
 *  Implementacion pura: sin LVGL, sin radio, sin Subjects, sin heap, sin reloj y
 *  sin acceso a la FFT. Todo lo que necesita entra por la instantanea
 *  (`dsp_autotune_spectrum_t`) y sale por los veredictos de abajo, asi que cada
 *  umbral de la seccion 5 y de la seccion 6 del issue es un hecho verificado por
 *  tests/native/test_autotune_spectrum.c con espectros sinteticos.
 *
 *  La regla que ordena todo el modulo: ante la duda, NO se mueve el dial (AM) y
 *  NO se propone nada (SSB). La resolucion fisica es `width_hz / 512` (el frame
 *  interpola 512 bins), y por eso no hay interpolacion subbin en ninguna parte.
 */

#include "autotune_spectrum.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* --- utilidades internas --------------------------------------------------- */

/* Division entera con redondeo al entero mas cercano; en los medios, alejandose
 * del cero. `den` es siempre > 0. */
static int32_t round_div_i32(int64_t num, int32_t den) {
    int64_t half = den / 2;

    if (num >= 0) {
        return (int32_t) ((num + half) / den);
    }
    return (int32_t) ((num - half) / den);
}

/* Mediana de n valores: copia local + insertion sort (n <= 800, sin heap y con
 * el mismo resultado en cualquier plataforma). n <= 0 devuelve 0. */
static float median_f32(const float *values, int n) {
    float buf[AUTOTUNE_SPECTRUM_BINS];

    if (values == NULL || n <= 0) {
        return 0.0f;
    }
    if (n > AUTOTUNE_SPECTRUM_BINS) {
        n = AUTOTUNE_SPECTRUM_BINS;
    }

    for (int i = 0; i < n; i++) {
        buf[i] = values[i];
    }
    for (int i = 1; i < n; i++) {
        float v = buf[i];
        int   j;

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

/* Mediana de las propuestas ya cuantizadas (n <= 5): misma copia + insertion
 * sort. Los candidatos son enteros y la mediana tambien. */
static int32_t median_i32(const int32_t *values, int n) {
    int32_t buf[8];

    if (values == NULL || n <= 0) {
        return 0;
    }
    if (n > 8) {
        n = 8;
    }

    for (int i = 0; i < n; i++) {
        buf[i] = values[i];
    }
    for (int i = 1; i < n; i++) {
        int32_t v = buf[i];
        int     j;

        for (j = i; j > 0 && buf[j - 1] > v; j--) {
            buf[j] = buf[j - 1];
        }
        buf[j] = v;
    }

    if (n % 2 == 1) {
        return buf[n / 2];
    }
    return round_div_i32((int64_t) buf[n / 2 - 1] + (int64_t) buf[n / 2], 2);
}

/* Ventana de bins del espectro con offsets en [lo, hi] (Hz), incluidos los
 * bordes. La mediana necesita materializar los valores: el buffer es del tamano
 * maximo del frame (3.2 KB en el stack, la misma disciplina que spectrum.c). */
static int window_collect(const dsp_autotune_spectrum_t *s, float lo, float hi, float *out,
                          int cap, bool *non_finite) {
    int n = 0;

    *non_finite = false;
    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        float off = autotune_spectrum_offset_hz(i, s->width_hz);

        if (off < lo || off > hi) {
            continue;
        }
        if (!isfinite(s->db[i])) {
            *non_finite = true;
            continue;
        }
        if (n < cap) {
            out[n++] = s->db[i];
        }
    }
    return n;
}

/* true si algun bin con |offset| <= `limit_hz` no es finito: la politica no
 * evalua nada con basura dentro de la zona que mira. */
static bool window_non_finite(const dsp_autotune_spectrum_t *s, float limit_hz) {
    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        float off = autotune_spectrum_offset_hz(i, s->width_hz);

        if (fabsf(off) <= limit_hz && !isfinite(s->db[i])) {
            return true;
        }
    }
    return false;
}

/* --- gate de la trama ------------------------------------------------------ */

void autotune_frame_gate_reset(autotune_frame_gate_t *g) {
    if (g == NULL) {
        return;
    }
    memset(g, 0, sizeof(*g));
}

void autotune_frame_gate_arm(autotune_frame_gate_t *g, uint64_t min_ts, uint64_t min_seq,
                             uint64_t epoch) {
    if (g == NULL) {
        return;
    }
    /* Una fase nueva arranca sin trama previa: el piso de secuencia y el de
     * tiempo se aplican desde la primera publicacion. */
    g->have_last = false;
    g->last_seq = 0;
    g->last_ts = 0;
    g->latched = false;
    g->epoch_set = (epoch != 0);
    g->epoch = epoch;
    g->min_ts = min_ts;
    g->min_seq = min_seq;
}

autotune_frame_verdict_t autotune_frame_gate_take(autotune_frame_gate_t *g,
                                                 const dsp_autotune_spectrum_t *s) {
    if (g == NULL || s == NULL) {
        return AUTOTUNE_FRAME_SKIP;
    }
    if (!s->valid || s->tx) {
        return AUTOTUNE_FRAME_SKIP;
    }
    /* Frescura: el vencimiento de un timer no prueba nada por si solo; la trama
     * tiene que ser POSTERIOR a la escritura (piso de tiempo), tener secuencia
     * nueva (fuera del descarte post-escritura) y timestamps estrictamente
     * crecientes. Una trama repetida o desordenada no entra. */
    if (s->timestamp_ms < g->min_ts) {
        return AUTOTUNE_FRAME_SKIP;
    }
    if (s->sequence <= g->min_seq) {
        return AUTOTUNE_FRAME_SKIP;
    }
    if (g->have_last) {
        if (s->sequence <= g->last_seq || s->timestamp_ms <= g->last_ts) {
            return AUTOTUNE_FRAME_SKIP;
        }
    }

    if (!g->latched) {
        if (g->epoch_set && s->epoch != g->epoch) {
            return AUTOTUNE_FRAME_SKIP;   /* publicacion anterior a la escritura propia */
        }
        g->epoch = s->epoch;
        g->center_freq_hz = s->center_freq_hz;
        g->mode = s->mode;
        g->width_hz = s->width_hz;
        g->latched = true;
    } else if (s->epoch != g->epoch || s->center_freq_hz != g->center_freq_hz ||
               s->mode != g->mode || s->width_hz != g->width_hz) {
        /* Otro epoch o ADEMAS otro centro/modo/ancho: la medicion no es
         * comparable con la que venia juntando. Nunca se mezclan dos contextos. */
        return AUTOTUNE_FRAME_BROKEN;
    }

    g->have_last = true;
    g->last_seq = s->sequence;
    g->last_ts = s->timestamp_ms;

    return AUTOTUNE_FRAME_TAKEN;
}

/* --- AM/SAM: portadora estrecha, unica y dominante ------------------------- */

bool autotune_spectrum_am_candidate(const dsp_autotune_spectrum_t *s, int32_t *offset_hz) {
    float   noise[AUTOTUNE_SPECTRUM_BINS];
    float   noise_db;
    float   resolution;
    float   step;
    float   peak_db = -INFINITY;
    float   peak_off = 0.0f;
    float   second_db;
    bool    have_second = false;
    bool    non_finite = false;
    int     i_peak = -1;
    int     n_noise;
    int32_t step_hz;

    if (offset_hz == NULL || !autotune_spectrum_measurable(s)) {
        return false;
    }

    resolution = autotune_spectrum_resolution_hz(s->width_hz);
    step_hz = autotune_spectrum_step_hz(s->width_hz);
    step = (float) step_hz;

    /* 1. Piso de ruido en los dos costados + pico de la ventana de portadora. */
    n_noise = window_collect(s, AUTOTUNE_AM_NOISE_MIN_HZ, AUTOTUNE_AM_NOISE_MAX_HZ, noise,
                             AUTOTUNE_SPECTRUM_BINS, &non_finite);
    if (non_finite || n_noise < AUTOTUNE_AM_NOISE_MIN_BINS) {
        return false;
    }
    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        float off = autotune_spectrum_offset_hz(i, s->width_hz);

        if (fabsf(off) > (float) AUTOTUNE_AM_CARRIER_WINDOW_HZ) {
            continue;
        }
        if (!isfinite(s->db[i])) {
            return false;
        }
        if (s->db[i] > peak_db) {
            peak_db = s->db[i];
            i_peak = i;
        }
    }
    if (i_peak < 0) {
        return false;
    }
    peak_off = autotune_spectrum_offset_hz(i_peak, s->width_hz);

    /* 2. Maximo local: ningun bin a distancia >= resolucion puede empatar (un
     * plateau de dos bins o una segunda portadora al mismo nivel no es un pico).
     * Y 3. segundo pico fuera de la guarda de +/-2*resolucion. */
    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        float off = autotune_spectrum_offset_hz(i, s->width_hz);
        float d;

        if (fabsf(off) > (float) AUTOTUNE_AM_CARRIER_WINDOW_HZ) {
            continue;
        }
        d = fabsf(off - peak_off);
        if (d >= resolution && s->db[i] >= peak_db) {
            return false;
        }
        if (d >= 2.0f * resolution && (!have_second || s->db[i] > second_db)) {
            second_db = s->db[i];
            have_second = true;
        }
    }

    noise_db = median_f32(noise, n_noise);
    if (peak_db - noise_db < AUTOTUNE_AM_PEAK_MIN_DB) {
        return false;   /* pico demasiado cerca del piso: no es una portadora */
    }
    if (!have_second) {
        second_db = noise_db;   /* sin segundo pico, el piso es la referencia */
    }
    if (peak_db - second_db < AUTOTUNE_AM_DOMINANCE_MIN_DB) {
        return false;   /* empate o interferencia: no hay portadora inequivoca */
    }

    /* 4. Candidato cuantizado al multiplo mas cercano de step_hz: sin
     * interpolacion subbin, la resolucion del PSD no lo permite. */
    {
        int32_t k = (int32_t) lroundf(peak_off / step);

        *offset_hz = (int32_t) ((int64_t) k * (int64_t) step_hz);
    }
    return true;
}

void autotune_am_reset(autotune_am_state_t *st) {
    if (st == NULL) {
        return;
    }
    memset(st, 0, sizeof(*st));
}

autotune_step_t autotune_am_take(autotune_am_state_t *st, const dsp_autotune_spectrum_t *s) {
    int32_t cand;
    float   min;
    float   max;

    if (st == NULL) {
        return AUTOTUNE_STEP_REJECTED;
    }
    if (!autotune_spectrum_am_candidate(s, &cand)) {
        return AUTOTUNE_STEP_REJECTED;
    }
    if (!st->have_resolution) {
        st->resolution_hz = autotune_spectrum_resolution_hz(s->width_hz);
        st->have_resolution = true;
    }

    /* Los candidatos de una misma fase comparten signo (cero se admite siempre):
     * una portadora que salta de lado no es una portadora. */
    if (cand != 0) {
        int32_t sign = (cand > 0) ? 1 : -1;

        if (st->sign == 0) {
            st->sign = sign;
        } else if (sign != st->sign) {
            return AUTOTUNE_STEP_REJECTED;
        }
    }

    if (st->n >= AUTOTUNE_AM_FRAMES) {
        for (int i = 1; i < AUTOTUNE_AM_FRAMES; i++) {
            st->offset_hz[i - 1] = st->offset_hz[i];
        }
        st->n = AUTOTUNE_AM_FRAMES - 1;
    }
    st->offset_hz[st->n++] = cand;

    if (st->n < AUTOTUNE_AM_FRAMES) {
        return AUTOTUNE_STEP_COLLECTING;
    }

    min = (float) st->offset_hz[0];
    max = (float) st->offset_hz[0];
    for (int i = 1; i < st->n; i++) {
        float v = (float) st->offset_hz[i];

        if (v < min) {
            min = v;
        }
        if (v > max) {
            max = v;
        }
    }
    if (max - min > st->resolution_hz) {
        return AUTOTUNE_STEP_COLLECTING;   /* inconsistente: la ventana sigue corriendo */
    }
    return AUTOTUNE_STEP_READY;
}

autotune_verdict_t autotune_am_verdict(const autotune_am_state_t *st, int32_t *delta_hz) {
    int32_t median;

    if (st == NULL || delta_hz == NULL || st->n < AUTOTUNE_AM_FRAMES) {
        return AUTOTUNE_VERDICT_NONE;
    }

    median = median_i32(st->offset_hz, st->n);
    if (fabsf((float) median) <= st->resolution_hz / 2.0f) {
        return AUTOTUNE_VERDICT_CENTERED;   /* ya centrado: cero escrituras */
    }
    if (median > AUTOTUNE_AM_MAX_DELTA_HZ || median < -AUTOTUNE_AM_MAX_DELTA_HZ) {
        return AUTOTUNE_VERDICT_NONE;
    }

    *delta_hz = median;
    return AUTOTUNE_VERDICT_MOVE;
}

bool autotune_spectrum_am_verified(const autotune_am_state_t *st) {
    if (st == NULL || st->n < AUTOTUNE_AM_FRAMES) {
        return false;
    }
    /* La consistencia (mismo signo y spread <= resolucion) ya la exigio take()
     * antes de dar READY; aca solo falta el criterio de la seccion 5.7: cuatro
     * tramas nuevas con la misma deteccion y |mediana| <= resolucion. */
    return fabsf((float) median_i32(st->offset_hz, st->n)) <= st->resolution_hz;
}

/* --- USB/LSB: energia ocupada dentro de la banda lateral ------------------- */

bool autotune_spectrum_ssb_filter_ok(int32_t from, int32_t to, bool voice_lsb) {
    int32_t width = to - from;

    if (width < AUTOTUNE_SSB_FILTER_MIN_HZ || width > AUTOTUNE_SSB_FILTER_MAX_HZ) {
        return false;
    }
    if (voice_lsb) {
        return from < to && to <= 0;
    }
    return from >= 0 && to > from;
}

/* Bordes de la banda RX desplazada `d` Hz, recortados al ancho de VOZ (300 a
 * 2700 Hz, signo de la banda lateral). La ventana es la del propio filtro; no se
 * toca ni se supone ningun otro. */
static void ssb_window(int32_t d, int32_t from, int32_t to, bool voice_lsb, float *lo, float *hi) {
    if (voice_lsb) {
        *lo = (float) d + (float) ((from > -2700) ? from : -2700);
        *hi = (float) d + (float) ((to < -300) ? to : -300);
    } else {
        *lo = (float) d + (float) ((from > 300) ? from : 300);
        *hi = (float) d + (float) ((to < 2700) ? to : 2700);
    }
}

/* Score de energia ocupada de una ventana: promedio de
 * clamp(db - noise - 3, 0, 24) sobre los bins de display de la ventana, o -1 si
 * la ventana no tiene los bins minimos. Es un indicador de ENERGIA, no de
 * calidad de voz. */
static float ssb_window_score(const dsp_autotune_spectrum_t *s, float lo, float hi, float noise_db) {
    float sum = 0.0f;
    int   n = 0;

    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        float off = autotune_spectrum_offset_hz(i, s->width_hz);
        float v;

        if (off < lo || off > hi) {
            continue;
        }
        v = s->db[i] - noise_db - 3.0f;
        if (v < 0.0f) {
            v = 0.0f;
        }
        if (v > 24.0f) {
            v = 24.0f;
        }
        sum += v;
        n++;
    }

    if (n < AUTOTUNE_SSB_MIN_WINDOW_BINS) {
        return -1.0f;
    }
    return sum / (float) n;
}

bool autotune_spectrum_ssb_candidate(const dsp_autotune_spectrum_t *s, int32_t from, int32_t to,
                                     bool voice_lsb, int32_t *delta_hz) {
    float   noise[AUTOTUNE_SPECTRUM_BINS];
    float   noise_db;
    float   resolution;
    float   best_score = -1.0f;
    float   second_score = 0.0f;
    float   score_zero = -1.0f;
    bool    non_finite = false;
    int     n_noise;
    int     k_max;
    int     step_hz;
    int32_t best_d = 0;

    if (delta_hz == NULL || !autotune_spectrum_measurable(s) ||
        !autotune_spectrum_ssb_filter_ok(from, to, voice_lsb)) {
        return false;
    }
    /* Toda la zona que se puntua y el piso de ruido: con basura adentro no se
     * decide nada. |d| <= 800 y el filtro <= 3500 dejan el limite en 5000. */
    if (window_non_finite(s, (float) AUTOTUNE_SSB_NOISE_MAX_HZ)) {
        return false;
    }

    resolution = autotune_spectrum_resolution_hz(s->width_hz);
    step_hz = autotune_spectrum_step_hz(s->width_hz);
    n_noise = window_collect(s, AUTOTUNE_SSB_NOISE_MIN_HZ, AUTOTUNE_SSB_NOISE_MAX_HZ, noise,
                             AUTOTUNE_SPECTRUM_BINS, &non_finite);
    if (non_finite || n_noise < AUTOTUNE_SSB_NOISE_MIN_BINS) {
        return false;
    }
    noise_db = median_f32(noise, n_noise);

    k_max = AUTOTUNE_SSB_MAX_DELTA_HZ / step_hz;
    for (int32_t k = -k_max; k <= k_max; k++) {
        int32_t d = k * step_hz;
        float   lo;
        float   hi;
        float   score;

        ssb_window(d, from, to, voice_lsb, &lo, &hi);
        score = ssb_window_score(s, lo, hi, noise_db);
        if (score < 0.0f) {
            continue;                       /* ventana vacia o sin bins suficientes */
        }
        if (d == 0) {
            score_zero = score;
        }
        if (score > best_score) {
            best_score = score;
            best_d = d;
        }
    }
    if (best_score < AUTOTUNE_SSB_MIN_SCORE || score_zero < 0.0f) {
        return false;
    }
    if (best_score < AUTOTUNE_SSB_RATIO_VS_ZERO * score_zero) {
        return false;   /* sin mejora clara contra no tocar nada */
    }

    /* Segundo mejor candidato separado del ganador por >= 2*resolucion. Sin
     * segundo candidato, 0 (nunca sugiere cuando el maximo esta empatado). */
    for (int32_t k = -k_max; k <= k_max; k++) {
        int32_t d = k * step_hz;
        float   lo;
        float   hi;
        float   score;

        if (fabsf((float) (d - best_d)) < 2.0f * resolution) {
            continue;
        }
        ssb_window(d, from, to, voice_lsb, &lo, &hi);
        score = ssb_window_score(s, lo, hi, noise_db);
        if (score > second_score) {
            second_score = score;
        }
    }
    if (best_score < AUTOTUNE_SSB_RATIO_VS_SECOND * second_score) {
        return false;   /* empate o dos maximos: no hay una propuesta unica */
    }

    /* Nunca cero ni un desplazamiento infimo, y nunca mas alla del tope. */
    if (best_d == 0 || best_d > AUTOTUNE_SSB_MAX_DELTA_HZ || best_d < -AUTOTUNE_SSB_MAX_DELTA_HZ) {
        return false;
    }
    {
        float min_delta = (resolution > (float) AUTOTUNE_SSB_MIN_DELTA_HZ) ? resolution
                                                                          : (float) AUTOTUNE_SSB_MIN_DELTA_HZ;
        if (fabsf((float) best_d) < min_delta) {
            return false;
        }
    }

    *delta_hz = best_d;
    return true;
}

void autotune_ssb_reset(autotune_ssb_state_t *st) {
    if (st == NULL) {
        return;
    }
    memset(st, 0, sizeof(*st));
}

autotune_step_t autotune_ssb_take(autotune_ssb_state_t *st, const dsp_autotune_spectrum_t *s,
                                  int32_t from, int32_t to, bool voice_lsb) {
    int32_t cand;
    float   spread_max;
    float   min;
    float   max;

    if (st == NULL) {
        return AUTOTUNE_STEP_REJECTED;
    }
    if (!autotune_spectrum_ssb_candidate(s, from, to, voice_lsb, &cand)) {
        return AUTOTUNE_STEP_REJECTED;
    }
    if (!st->have_resolution) {
        st->resolution_hz = autotune_spectrum_resolution_hz(s->width_hz);
        st->have_resolution = true;
    }

    {
        int32_t sign = (cand > 0) ? 1 : -1;

        if (st->sign == 0) {
            st->sign = sign;
        } else if (sign != st->sign) {
            return AUTOTUNE_STEP_REJECTED;
        }
    }

    if (st->n >= AUTOTUNE_SSB_FRAMES) {
        for (int i = 1; i < AUTOTUNE_SSB_FRAMES; i++) {
            st->delta_hz[i - 1] = st->delta_hz[i];
        }
        st->n = AUTOTUNE_SSB_FRAMES - 1;
    }
    st->delta_hz[st->n++] = cand;

    if (st->n < AUTOTUNE_SSB_FRAMES) {
        return AUTOTUNE_STEP_COLLECTING;
    }

    min = (float) st->delta_hz[0];
    max = (float) st->delta_hz[0];
    for (int i = 1; i < st->n; i++) {
        float v = (float) st->delta_hz[i];

        if (v < min) {
            min = v;
        }
        if (v > max) {
            max = v;
        }
    }
    spread_max = ((float) AUTOTUNE_SSB_MIN_DELTA_HZ > st->resolution_hz)
                     ? (float) AUTOTUNE_SSB_MIN_DELTA_HZ
                     : st->resolution_hz;
    if (max - min > spread_max) {
        return AUTOTUNE_STEP_COLLECTING;
    }
    return AUTOTUNE_STEP_READY;
}

autotune_verdict_t autotune_ssb_verdict(const autotune_ssb_state_t *st, int32_t *delta_hz) {
    int32_t median;
    float   min_delta;

    if (st == NULL || delta_hz == NULL || st->n < AUTOTUNE_SSB_FRAMES) {
        return AUTOTUNE_VERDICT_NONE;
    }

    median = median_i32(st->delta_hz, st->n);
    min_delta = (st->resolution_hz > (float) AUTOTUNE_SSB_MIN_DELTA_HZ)
                    ? st->resolution_hz
                    : (float) AUTOTUNE_SSB_MIN_DELTA_HZ;
    if (median > AUTOTUNE_SSB_MEDIAN_MAX_HZ || median < -AUTOTUNE_SSB_MEDIAN_MAX_HZ) {
        return AUTOTUNE_VERDICT_NONE;
    }
    if (fabsf((float) median) < min_delta) {
        return AUTOTUNE_VERDICT_NONE;
    }

    *delta_hz = median;
    return AUTOTUNE_VERDICT_SUGGEST;
}

/* --- ventanas y pulsaciones ------------------------------------------------ */

uint64_t autotune_phase_deadline(autotune_phase_t phase, uint64_t now_ms) {
    switch (phase) {
        case AUTOTUNE_PHASE_COLLECT:
            return now_ms + AUTOTUNE_COLLECT_MS;
        case AUTOTUNE_PHASE_VERIFY:
            return now_ms + AUTOTUNE_VERIFY_MS;
        case AUTOTUNE_PHASE_PREVIEW:
            return now_ms + AUTOTUNE_PREVIEW_MS;
        case AUTOTUNE_PHASE_UNDO:
            return now_ms + AUTOTUNE_UNDO_MS;
        default:
            return now_ms;
    }
}

bool autotune_phase_expired(uint64_t deadline_ms, uint64_t now_ms) {
    return now_ms >= deadline_ms;
}

bool autotune_phase_measures(autotune_phase_t phase) {
    return phase == AUTOTUNE_PHASE_COLLECT || phase == AUTOTUNE_PHASE_VERIFY;
}

autotune_press_t autotune_phase_press(autotune_phase_t phase) {
    switch (phase) {
        case AUTOTUNE_PHASE_PREVIEW:
            return AUTOTUNE_PRESS_APPLY;
        case AUTOTUNE_PHASE_UNDO:
            return AUTOTUNE_PRESS_UNDO;
        default:
            return AUTOTUNE_PRESS_CANCEL;
    }
}

bool autotune_phase_restores_on_cancel(autotune_phase_t phase) {
    return phase == AUTOTUNE_PHASE_VERIFY;
}

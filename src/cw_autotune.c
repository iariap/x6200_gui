/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Auto-ajuste del decodificador CW (issue #139)
 *
 *  Ver `cw_autotune.h` para la API y el porque del modulo aparte. Las formulas
 *  y la tabla de casos estan en `docs/cw139-spec.md`; aca va el comentario de
 *  cada una con su numero.
 */

#include "cw_autotune.h"

#include <math.h>
#include <string.h>

static float clampf(float x, float lo, float hi) {
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

/* Los valores se cuantizan al paso con el que el operador los ve y con el que
 * se persisten (SNR 0,1 dB; betas 0,01; gist 0,1 — src/cfg/cfg.c): asi el
 * rotulo del boton muestra exactamente lo aplicado y un reinicio no lo redondea
 * a otro numero. */
static float quantize(float x, float step) {
    return roundf(x / step) * step;
}

static void echo_applied(const cw_autotune_state_t *st, cw_autotune_result_t *out) {
    memset(out, 0, sizeof(*out));

    if (st != NULL && st->have_applied) {
        out->snr = st->snr;
        out->gist = st->gist;
        out->peak_beta = st->peak_beta;
        out->noise_beta = st->noise_beta;
        out->freq_offset_hz = st->freq_offset_hz;
    }
}

/* Promedia con la sugerencia anterior cuando el salto supera dos bandas
 * muertas: un QSB o un golpe de ruido no se persigue de una ventana a la otra. */
static float average_if_jump(float value, float previous, float band) {
    if (fabsf(value - previous) > band * CW_AUTOTUNE_JUMP_FACTOR) {
        return (value + previous) / 2.0f;
    }
    return value;
}

float cw_autotune_tau_peak_ms(int wpm) {
    if (wpm <= 0) {
        wpm = CW_AUTOTUNE_WPM_DEFAULT;
    }
    if (wpm < CW_AUTOTUNE_WPM_MIN) {
        wpm = CW_AUTOTUNE_WPM_MIN;
    }
    if (wpm > CW_AUTOTUNE_WPM_MAX) {
        wpm = CW_AUTOTUNE_WPM_MAX;
    }
    /* Medio punto Morse: el punto dura 1200/WPM ms (PARIS). */
    return 0.5f * CW_AUTOTUNE_DOT_MS(wpm);
}

float cw_autotune_percentile(const float *values, int n, float q) {
    float tmp[CW_AUTOTUNE_MAX_WINDOW];
    int   i, j;

    if (values == NULL || n <= 0) {
        return 0.0f;
    }
    if (n > CW_AUTOTUNE_MAX_WINDOW) {
        n = CW_AUTOTUNE_MAX_WINDOW;
    }
    if (q < 0.0f) {
        q = 0.0f;
    }
    if (q > 1.0f) {
        q = 1.0f;
    }

    for (i = 0; i < n; i++) {
        tmp[i] = values[i];
    }
    for (i = 1; i < n; i++) {
        float v = tmp[i];

        for (j = i; j > 0 && tmp[j - 1] > v; j--) {
            tmp[j] = tmp[j - 1];
        }
        tmp[j] = v;
    }

    /* Rango mas cercano: con n=10 y q=0,9 el indice 8 es el noveno valor. */
    i = (int) ceilf(q * (float) n) - 1;
    if (i < 0) {
        i = 0;
    }
    if (i > n - 1) {
        i = n - 1;
    }
    return tmp[i];
}

float cw_autotune_mean(const float *values, int n) {
    float sum = 0.0f;
    int   i;

    if (values == NULL || n <= 0) {
        return 0.0f;
    }
    for (i = 0; i < n; i++) {
        sum += values[i];
    }
    return sum / (float) n;
}

float cw_autotune_variance(const float *values, int n) {
    float mean, sum = 0.0f;
    int   i;

    if (values == NULL || n <= 0) {
        return 0.0f;
    }
    mean = cw_autotune_mean(values, n);
    for (i = 0; i < n; i++) {
        float d = values[i] - mean;

        sum += d * d;
    }
    return sum / (float) n;
}

void cw_autotune_reset(cw_autotune_state_t *st) {
    if (st != NULL) {
        memset(st, 0, sizeof(*st));
    }
}

void cw_autotune_compute(cw_autotune_state_t *st, const cw_autotune_stats_t *in,
                         cw_autotune_result_t *out) {
    float observed_db;
    float snr, gist, tau_ms, peak_beta, noise_beta;
    float sigma;
    float freq, target, step;
    bool  enough;

    if (out == NULL) {
        return;
    }
    echo_applied(st, out);
    if (st == NULL || in == NULL) {
        return;
    }

    /* Guardas: la ventana tiene marca, el piso y la marca son numeros, hay mas
     * senal que ruido y el margen alcanza para que la sugerencia no persiga
     * ruido. Sin esto no se escribe NADA (ni se pierde lo manual). */
    enough = in->mark_valid && isfinite(in->mark_db) && isfinite(in->noise_db)
             && in->signal_ratio > CW_AUTOTUNE_MIN_SIGNAL_RATIO;
    if (!enough) {
        return;
    }
    observed_db = in->mark_db - in->noise_db;
    if (!isfinite(observed_db) || observed_db < CW_AUTOTUNE_MIN_OBSERVED_SNR_DB) {
        return;
    }
    out->adapt = true;

    /* snr: punto medio entre marca y piso, con techo 10 dB (el margen de
     * decodificacion no mejora por encima) y el clamp del MFK 3..30. */
    snr = clampf(0.5f * observed_db, CW_AUTOTUNE_SNR_MIN, CW_AUTOTUNE_SNR_SOFT_MAX);
    snr = clampf(quantize(snr, 0.1f), CW_AUTOTUNE_SNR_MIN, CW_AUTOTUNE_SNR_MAX);

    /* gist: histeresis proporcional al margen; 0,25 * snr da 0,75..2,5 dB en el
     * rango util, con el clamp 0,5..3 del default (1,0 dB). */
    gist = clampf(0.25f * snr, CW_AUTOTUNE_GIST_MIN, CW_AUTOTUNE_GIST_MAX);
    gist = clampf(quantize(gist, 0.1f), CW_AUTOTUNE_GIST_MIN, CW_AUTOTUNE_GIST_MAX);

    /* peak_beta: retencion del LPF del umbral de pico para una constante de
     * tiempo de medio punto. beta = 1 - delta/tau (x[n] = beta*x[n-1] +
     * (1-beta)*hoy). Con el bloque real de 185,76 ms el resultado cae siempre
     * en el clamp inferior (ver HALLAZGOS 53): 0,10 es el ajuste mas rapido que
     * el bloque permite, y es el default del firmware. */
    tau_ms = cw_autotune_tau_peak_ms(in->wpm);
    peak_beta = CW_AUTOTUNE_BETA_MIN;
    if (in->block_dt_s > 0.0f) {
        peak_beta = 1.0f - (in->block_dt_s * 1000.0f) / tau_ms;
    }
    peak_beta = clampf(quantize(peak_beta, 0.01f), CW_AUTOTUNE_BETA_MIN, CW_AUTOTUNE_BETA_MAX);

    /* noise_beta: un piso nervioso (sigma grande) se sigue rapido (0,5..0,7);
     * uno estable se suaviza (0,8..0,9). La recta 0,90 - 0,10*(sigma-1) da
     * 0,90 en 1 dB, 0,70 en 3 dB y 0,50 en 5 dB. */
    sigma = (in->noise_var_db2 > 0.0f) ? sqrtf(in->noise_var_db2) : 0.0f;
    noise_beta = 0.90f - 0.10f * (sigma - 1.0f);
    noise_beta = clampf(noise_beta, 0.50f, 0.90f);
    noise_beta = clampf(quantize(noise_beta, 0.01f), CW_AUTOTUNE_BETA_MIN, CW_AUTOTUNE_BETA_MAX);

    /* Promedio de saltos entre ventanas (antes de la banda muerta). */
    if (st->have_suggested) {
        snr = average_if_jump(snr, st->last_snr, CW_AUTOTUNE_SNR_DEADBAND_DB);
        gist = average_if_jump(gist, st->last_gist, CW_AUTOTUNE_GIST_DEADBAND);
        peak_beta = average_if_jump(peak_beta, st->last_peak_beta, CW_AUTOTUNE_BETA_DEADBAND);
        noise_beta = average_if_jump(noise_beta, st->last_noise_beta, CW_AUTOTUNE_BETA_DEADBAND);
        /* El promedio de dos valores cuantizados puede caer entre pasos: se
         * vuelve a cuantizar para que lo aplicado siga siendo visible. */
        snr = clampf(quantize(snr, 0.1f), CW_AUTOTUNE_SNR_MIN, CW_AUTOTUNE_SNR_MAX);
        gist = clampf(quantize(gist, 0.1f), CW_AUTOTUNE_GIST_MIN, CW_AUTOTUNE_GIST_MAX);
        peak_beta = clampf(quantize(peak_beta, 0.01f), CW_AUTOTUNE_BETA_MIN, CW_AUTOTUNE_BETA_MAX);
        noise_beta = clampf(quantize(noise_beta, 0.01f), CW_AUTOTUNE_BETA_MIN, CW_AUTOTUNE_BETA_MAX);
    }
    st->last_snr = snr;
    st->last_gist = gist;
    st->last_peak_beta = peak_beta;
    st->last_noise_beta = noise_beta;
    st->have_suggested = true;

    /* Banda muerta contra lo aplicado: solo se escribe si el cambio supera el
     * 10 % del rango (o si es la primera ventana: calibracion inmediata). */
    out->apply_snr = !st->have_applied || fabsf(snr - st->snr) > CW_AUTOTUNE_SNR_DEADBAND_DB;
    out->apply_gist = !st->have_applied || fabsf(gist - st->gist) > CW_AUTOTUNE_GIST_DEADBAND;
    out->apply_peak_beta = !st->have_applied || fabsf(peak_beta - st->peak_beta) > CW_AUTOTUNE_BETA_DEADBAND;
    out->apply_noise_beta = !st->have_applied || fabsf(noise_beta - st->noise_beta) > CW_AUTOTUNE_BETA_DEADBAND;

    if (out->apply_snr) {
        st->snr = snr;
    }
    if (out->apply_gist) {
        st->gist = gist;
    }
    if (out->apply_peak_beta) {
        st->peak_beta = peak_beta;
    }
    if (out->apply_noise_beta) {
        st->noise_beta = noise_beta;
    }
    out->snr = st->snr;
    out->gist = st->gist;
    out->peak_beta = st->peak_beta;
    out->noise_beta = st->noise_beta;

    /* Seguimiento del tono: la correccion es -peak_freq (HALLAZGOS 52), se
     * acumula sobre lo ya aplicado, se acota a +/-300 Hz y se mueve como mucho
     * 30 Hz por ventana (lento). Banda muerta de 10 Hz: un bloque del widget. */
    freq = st->have_applied ? st->freq_offset_hz : 0.0f;
    out->freq_offset_hz = freq;
    if (in->peak_valid && isfinite(in->peak_freq_hz)) {
        target = clampf(freq - in->peak_freq_hz, -CW_AUTOTUNE_FREQ_LIMIT_HZ,
                        CW_AUTOTUNE_FREQ_LIMIT_HZ);
        if (st->have_suggested) {
            target = average_if_jump(target, st->last_freq, CW_AUTOTUNE_FREQ_DEADBAND_HZ);
        }
        st->last_freq = target;

        step = target - freq;
        if (fabsf(step) > CW_AUTOTUNE_FREQ_DEADBAND_HZ) {
            step = clampf(step, -CW_AUTOTUNE_FREQ_STEP_HZ, CW_AUTOTUNE_FREQ_STEP_HZ);
            freq += step;
            out->apply_freq = true;
            st->freq_offset_hz = freq;
            out->freq_offset_hz = freq;
        }
    }

    st->have_applied = true;
}

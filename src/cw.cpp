/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */
#include "cw.h"


#include <atomic>
#include <math.h>

#include "util.h"
#include "cfg/cfg.h"
#include "cfg/subjects.h"

extern "C" {
    #include "lvgl/lvgl.h"
    #include <pthread.h>
    #include "audio.h"
    #include "params/params.h"
    #include "cw_decoder.h"
    #include "cw_autotune.h"
    #include "pannel.h"
    #include "meter.h"
    #include "cw_tune_ui.h"
    #include "pubsub_ids.h"
}

typedef struct {
    uint16_t    n;
    float       val;
} fft_item_t;

#define NUM_STAGES 6
#define DECIM_FACTOR (1LL << NUM_STAGES)
#define FFT 128
#define MAX_CW_BW 500

static bool ready = false;

static fft_item_t fft_items[FFT];

static dds_cccf  ds_dec;
static wrms_t    wrms;
static cbuffercf input_cbuf;
static cbuffercf rms_cbuf;
static wdelayf   rms_delay;

static cbuffercf fft_cbuf;
static fftplan   fft_plan;
static float     window[FFT];
static cfloat    fft_time[FFT];
static cfloat    fft_freq[FFT];
static float     audio_psd_squared[FFT];

static float peak_filtered;
static float noise_filtered;
static float threshold_pulse;
static float threshold_silence;
static float rms_db_max;
static float rms_db_min;
static bool  peak_on = false;

static int32_t key_tone = 0;
static float   cw_decoder_peak_beta;
static float   cw_decoder_noise_beta;
static float   cw_decoder_snr;
static float   cw_decoder_snr_gist;
static bool    cw_decoder;
static bool                 trainer = false;

/* AUTO (issue #139). La ventana acumula una muestra por trama de FFT (unas
 * 5,4 por segundo): nivel de marca, piso de ruido y offset del tono. Se
 * acumula siempre (es barato y deja una ventana lista para la calibracion
 * inmediata al encender AUTO); se evalua cada ~5 s y solo con AUTO encendido. */
#define AUTOTUNE_WINDOW_MS  5000
#define AUTOTUNE_MIN_MARK   3

static cw_autotune_state_t at_state;

static float    at_mark[CW_AUTOTUNE_MAX_WINDOW];
static float    at_noise[CW_AUTOTUNE_MAX_WINDOW];
static float    at_peak[CW_AUTOTUNE_MAX_WINDOW];
static int      at_mark_n;
static int      at_noise_n;
static int      at_peak_n;
static float    at_ratio;
static bool     at_frame_signal;
static bool     at_first_threshold = true;
static bool     at_window_open;
static uint64_t at_window_start;

/* El AUTO los escribe el hilo de la GUI (boton/observador) y los lee el hilo
 * del DSP: atomicos para no depender del azar. */
static std::atomic<bool> cw_decoder_auto(false);
static std::atomic<bool> at_request(false);

/* Corrimiento del centro del DDS del decodificador RX respecto del tono manual
 * (`key_tone`). Es SOLO del receptor: `cfg.key_tone` —el tono de TX, que la
 * radio usa para el sidetone— no se toca nunca desde AUTO. */
static float rx_tone_offset;

static pthread_mutex_t  cw_mutex = PTHREAD_MUTEX_INITIALIZER;

static void dds_dec_init();

static void on_key_tone_change(Subject *subj, void *user_data);
static void on_auto_change(Subject *subj, void *user_data);
static void on_val_float_change(Subject *subj, void *user_data);
static void on_val_bool_change(Subject *subj, void *user_data);

void cw_trainer_set(bool on) {
    trainer = on;
}

void cw_init() {
    cfg.key_tone.val->subscribe(on_key_tone_change)->notify();
    cfg.cw_decoder_peak_beta.val->subscribe(on_val_float_change, (void*)&cw_decoder_peak_beta)->notify();
    cfg.cw_decoder_noise_beta.val->subscribe(on_val_float_change, (void*)&cw_decoder_noise_beta)->notify();
    cfg.cw_decoder_snr.val->subscribe(on_val_float_change, (void*)&cw_decoder_snr)->notify();
    cfg.cw_decoder_snr_gist.val->subscribe(on_val_float_change, (void*)&cw_decoder_snr_gist)->notify();
    cfg.cw_decoder.val->subscribe(on_val_bool_change, (void*)&cw_decoder)->notify();
    cfg.cw_decoder_auto.val->subscribe(on_auto_change)->notify();
    cw_autotune_reset(&at_state);
    /* `cfg.cw_tune` ya no se observa aca: el parametro del boton "Tuner" no gatea ni la visibilidad
     * del widget (src/cw_tune_ui.c) ni este feed (issue #122). El boton sigue existiendo. */

    input_cbuf = cbuffercf_create(10000);
    wrms = wrms_create(16, 4);
    rms_cbuf = cbuffercf_create(4000 / 8 * 2);
    fft_cbuf = cbuffercf_create(4000 / 8 * 2);

    // Window for FFT
    float scale = 0.0f;
    for (size_t i = 0; i < FFT; i++) {
        window[i] = liquid_hann(i, FFT);
        scale += window[i] * window[i];
    }
    scale = 1.0f / sqrtf(scale);
    // scale window and copy
    for (size_t i=0; i<FFT; i++)
        window[i] *= scale;

    fft_plan = fft_create_plan(FFT, fft_time, fft_freq, LIQUID_FFT_FORWARD, 0);

    rms_delay = wdelayf_create(FFT / wrms_delay(wrms));

    peak_filtered = -10.0f;
    noise_filtered = -20.0f;

    ready = true;
}

static void dds_dec_init() {
    pthread_mutex_lock(&cw_mutex);
    if (ds_dec != NULL) {
        dds_cccf_destroy(ds_dec);
    }
    /* El centro del DDS es el tono manual mas el corrimiento que haya medido
     * AUTO; con AUTO apagado el corrimiento es 0 y el DDS queda como siempre. */
    float rel_freq = ((float) key_tone + rx_tone_offset) / AUDIO_CAPTURE_RATE;
    float bw = (float) MAX_CW_BW / AUDIO_CAPTURE_RATE;
    ds_dec = dds_cccf_create(NUM_STAGES, rel_freq, bw, 60.0f);
    pthread_mutex_unlock(&cw_mutex);
}

/* --- AUTO (issue #139): ventana de estadisticas y aplicacion ----------------- */

static void autotune_push(float *buf, int *n, float value) {
    if (*n < CW_AUTOTUNE_MAX_WINDOW) {
        buf[(*n)++] = value;
    }
}

static void autotune_window_reset(uint64_t now) {
    at_mark_n = 0;
    at_noise_n = 0;
    at_peak_n = 0;
    at_ratio = 0.0f;
    at_frame_signal = false;
    at_window_start = now;
    at_window_open = true;
}

/* Una ventana valida -> sugerencia -> subjects (los mismos que usan los botones)
 * y, si corresponde, el centro del DDS del decodificador RX. Nada mas: el modo,
 * el tono de TX y el resto de la configuracion no se tocan. */
static void autotune_apply() {
    cw_autotune_stats_t  stats = {};
    cw_autotune_result_t result;
    int                  wpm;

    stats.noise_db       = cw_autotune_mean(at_noise, at_noise_n);
    stats.mark_db        = cw_autotune_percentile(at_mark, at_mark_n, 0.90f);
    stats.noise_var_db2  = cw_autotune_variance(at_noise, at_noise_n);
    stats.mark_var_db2   = cw_autotune_variance(at_mark, at_mark_n);
    stats.signal_ratio   = at_ratio;
    /* Bloque de FFT: 128 muestras a AUDIO_CAPTURE_RATE/DECIM_FACTOR = 689,06 Hz. */
    stats.block_dt_s     = ((float) FFT) / ((float) AUDIO_CAPTURE_RATE / (float) DECIM_FACTOR);
    stats.peak_freq_hz   = cw_autotune_percentile(at_peak, at_peak_n, 0.5f);
    stats.peak_valid     = at_peak_n > 0;
    stats.mark_valid     = at_mark_n > 0;

    /* La WPM medida por el decodificador manda; si todavia no midio (0) o se
     * fue de rango, se usa la velocidad configurada (la del keyer). */
    wpm = cw_decoder_get_wpm();
    if (wpm < CW_AUTOTUNE_WPM_MIN || wpm > CW_AUTOTUNE_WPM_MAX) {
        wpm = cw_get_wpm();
    }
    stats.wpm = wpm;

    cw_autotune_compute(&at_state, &stats, &result);
    if (!result.adapt) {
        return;
    }
    if (result.apply_snr) {
        subject_set_float(cfg.cw_decoder_snr.val, result.snr);
    }
    if (result.apply_gist) {
        subject_set_float(cfg.cw_decoder_snr_gist.val, result.gist);
    }
    if (result.apply_peak_beta) {
        subject_set_float(cfg.cw_decoder_peak_beta.val, result.peak_beta);
    }
    if (result.apply_noise_beta) {
        subject_set_float(cfg.cw_decoder_noise_beta.val, result.noise_beta);
    }
    if (result.apply_freq) {
        rx_tone_offset = result.freq_offset_hz;
        dds_dec_init();
    }
}

/* Corre una vez por trama de FFT (~5,4 Hz). Evalua al cumplirse la ventana de
 * 5 s o cuando el boton acaba de encender AUTO (calibracion inmediata). Con
 * AUTO apagado solo rota la ventana: no escribe nada. */
static void autotune_tick() {
    uint64_t now     = get_time();
    bool     request = at_request.exchange(false);

    if (!at_window_open) {
        autotune_window_reset(now);
        return;
    }
    if (!request && (now - at_window_start) < AUTOTUNE_WINDOW_MS) {
        return;
    }
    if (cw_decoder_auto.load() && at_mark_n >= AUTOTUNE_MIN_MARK && at_noise_n > 0) {
        autotune_apply();
    }
    autotune_window_reset(now);
}

static int compare_fft_items(const void *p1, const void *p2) {
    fft_item_t *i1 = (fft_item_t *) p1;
    fft_item_t *i2 = (fft_item_t *) p2;

    return (i1->val > i2->val) ? -1 : 1;
}

static void update_thresholds() {
    float noise;
    float sum_all = 0.0f;
    float sum_signal = 0.0f;
    float peak_val = -1.0f;
    int16_t peak_pos, peak_start, peak_end;

    // Find top positions
    for (int16_t n = 0; n < FFT; n++) {
        if (audio_psd_squared[n] > peak_val) {
            peak_val = audio_psd_squared[n];
            peak_pos = n;
        }
        sum_all += audio_psd_squared[n];
    }

    // BW for 30 WPM
    uint16_t peak_width = 30 * 4 * FFT * DECIM_FACTOR / AUDIO_CAPTURE_RATE;

    peak_start = peak_pos - peak_width / 2;
    peak_end = peak_start + peak_width;
    if (peak_start < 0) {
        peak_start = 0;
        peak_end = peak_width;
    } else if (peak_end >= FFT) {
        peak_end = FFT;
        peak_start = peak_end - peak_width;
    }

    for (uint16_t i = peak_start; i < peak_end; i++) {
        sum_signal += audio_psd_squared[i];
    }
    noise = sum_all - sum_signal;

    float ratio = sum_signal/noise;

    if (ratio > 1) {
        lpf(&peak_filtered, LV_MAX(noise_filtered + cw_decoder_snr, rms_db_max), cw_decoder_peak_beta, S_MIN);
        threshold_pulse = 0;
        /* Muestra de marca de la ventana AUTO: el maximo RMS entre esta trama y
         * la anterior. Los sentinelas (-121/-127) no son niveles: se filtran. */
        at_frame_signal = true;
        /* La mejor relacion de la ventana, no la de la ultima trama: una
         * ventana de 5 s que termina en un espacio sigue teniendo senal. */
        if (ratio > at_ratio) {
            at_ratio = ratio;
        }
        /* La primera trama lee rms_db_max=0 (el cero de las estaticas), no un
         * nivel: no entra en la ventana. No se toca el valor que usa el
         * decodificador (sin AUTO el umbral queda como siempre). */
        if (!at_first_threshold && rms_db_max > -100.0f && rms_db_max < 20.0f) {
            autotune_push(at_mark, &at_mark_n, rms_db_max);
        }
    } else {
        lpf(&noise_filtered, LV_MIN(-3.0f, rms_db_min), cw_decoder_noise_beta, S_MIN);
        peak_filtered -= 0.3f;
        if (noise_filtered + cw_decoder_snr > peak_filtered) {
            peak_filtered = noise_filtered + cw_decoder_snr;
        }
        // Increase threshold for no signal
        threshold_pulse = 1;
        /* Muestra del piso: el valor crudo que alimenta al LPF de ruido. */
        at_frame_signal = false;
        if (LV_MIN(-3.0f, rms_db_min) > -100.0f) {
            autotune_push(at_noise, &at_noise_n, LV_MIN(-3.0f, rms_db_min));
        }
    }
    float low = noise_filtered + cw_decoder_snr;
    threshold_pulse += LV_MAX(low, peak_filtered  - 3.0f);
    threshold_silence = threshold_pulse - cw_decoder_snr_gist;
    at_first_threshold = false;
    rms_db_min = 0.0f;
    rms_db_max = S1;
}

static void update_peak_freq(float freq) {
    /* Muestra de tono de la ventana AUTO, solo en tramas con senal. El rango es
     * el del mapeo de la FFT (+/-344 Hz): fuera de eso no es un tono. */
    if (at_frame_signal && freq > -400.0f && freq < 400.0f) {
        autotune_push(at_peak, &at_peak_n, freq);
    }
    if (peak_on) {
        cw_tune_set_freq(freq);
    }
}

static bool decode(float rms_db) {
    if (peak_on) {
        if (rms_db < threshold_silence) {
            peak_on = false;
        }
    } else {
        if (rms_db > threshold_pulse) {
            peak_on = true;
        }
    }
    // printf("CW_levels%f,%f,%f,%f\n", rms_db, noise_filtered, peak_filtered, threshold_pulse);
    return peak_on;
}


void cw_put_audio_samples(unsigned int n, cfloat *samples) {
    if (!ready) {
        return;
    }
    /* El feed alimenta al decodificador CW, al trainer y al widget de sintonia (mas abajo,
     * update_peak_freq -> cw_tune_set_freq, que mueve el bloque que marca donde esta el tono):
     * corre si alguno lo necesita. Antes miraba `cw_tune` —el parametro del boton "Tuner", que
     * arranca en 0 (src/cfg/cfg.c:349)—: con el widget ya visible en CW/CWR sin depender de ese
     * parametro (issue #122), el gate sigue a la visibilidad del widget, que es lo que el feed
     * tiene que alimentar. Esta funcion solo se llama en CW/CWR (src/dsp.cpp:251), que es
     * exactamente donde el widget se muestra, asi que el feed corre siempre que se lo pueda ver. */
    if ((!cw_decoder) && (!trainer) && (!cw_tune_ui_visible())) {
        return;
    }
    cfloat sample;
    float rms_db, peak_freq;
    size_t max_pos;

    // fill input buffer
    cbuffercf_write(input_cbuf, samples, n);

    // Downsample and freq shift
    const size_t desired_num = (1<<NUM_STAGES);
    while (cbuffercf_size(input_cbuf) >= desired_num) {
        unsigned int n;
        cfloat *buf;
        cbuffercf_read(input_cbuf, desired_num, &buf, &n);
        pthread_mutex_lock(&cw_mutex);
        dds_cccf_decim_execute(ds_dec, buf, &sample);
        pthread_mutex_unlock(&cw_mutex);
        cbuffercf_release(input_cbuf, desired_num);
        cbuffercf_push(rms_cbuf, sample);
        cbuffercf_push(fft_cbuf, sample);

        // Process FFT
        while (cbuffercf_size(fft_cbuf) >= FFT) {
            for (size_t i = 0; i < FFT; i++) {
                cbuffercf_pop(fft_cbuf, &fft_time[i]);
                fft_time[i] *= window[i];
            }
            fft_execute(fft_plan);

            for (size_t i = 0; i < FFT; i++) {
                audio_psd_squared[i] = std::real(fft_freq[i] * std::conj(fft_freq[i]));
            }
            update_thresholds();

            max_pos = argmax(audio_psd_squared, FFT);
            // Fix fft order
            peak_freq = (((float) (FFT - (max_pos + FFT / 2) % FFT) / FFT) - 0.5f) * ((float) AUDIO_CAPTURE_RATE / DECIM_FACTOR);
            update_peak_freq(peak_freq);
            autotune_tick();
        }

        // Process RMS
        while (cbuffercf_size(rms_cbuf)) {
            cbuffercf_pop(rms_cbuf, &sample);
            wrms_pushcf(wrms, sample);
            if (wrms_ready(wrms)) {
                rms_db = wrms_get_val(wrms);
                rms_db_min = LV_MIN(rms_db_min, rms_db);
                rms_db_max = LV_MAX(rms_db_max, rms_db);
                wdelayf_push(rms_delay, rms_db);
                wdelayf_read(rms_delay, &rms_db);
                cw_decoder_signal(decode(rms_db), 1000.0f / AUDIO_CAPTURE_RATE * DECIM_FACTOR * wrms_delay(wrms));
            }
        }
    }
}

// bool cw_change_decoder(int16_t df) {
//     if (df == 0) {
//         return params.cw_decoder;
//     }

//     params_lock();
//     params.cw_decoder = !params.cw_decoder;
//     params_unlock(&params.dirty.cw_decoder);
//     lv_msg_send(MSG_PARAM_CHANGED, NULL);

//     pannel_visible();

//     return params.cw_decoder;
// }

// float cw_change_snr(int16_t df) {
//     if (df == 0) {
//         return params.cw_decoder_snr;
//     }

//     float x = params.cw_decoder_snr + df * 0.1f;

//     if (x < 3.0f) {
//         x = 3.0f;
//     } else if (x > 30.0f) {
//         x = 30.0f;
//     }

//     params_lock();
//     params.cw_decoder_snr = x;
//     params_unlock(&params.dirty.cw_decoder_snr);
//     lv_msg_send(MSG_PARAM_CHANGED, NULL);

//     return params.cw_decoder_snr;
// }

// float cw_change_peak_beta(int16_t df) {
//     if (df == 0) {
//         return params.cw_decoder_peak_beta;
//     }

//     float x = params.cw_decoder_peak_beta + df * 0.01f;

//     if (x < 0.1f) {
//         x = 0.1f;
//     } else if (x > 0.95f) {
//         x = 0.95f;
//     }

//     params_lock();
//     params.cw_decoder_peak_beta = x;
//     params_unlock(&params.dirty.cw_decoder_peak_beta);
//     lv_msg_send(MSG_PARAM_CHANGED, NULL);

//     return params.cw_decoder_peak_beta;
// }

static void on_key_tone_change(Subject *subj, void *user_data) {
    key_tone = static_cast<SubjectT<int32_t>*>(subj)->get();
    dds_dec_init();
}

static void on_val_float_change(Subject *subj, void *user_data) {
    *(float*)user_data = static_cast<SubjectT<float>*>(subj)->get();
}

static void on_val_bool_change(Subject *subj, void *user_data) {
    *(bool*)user_data = static_cast<SubjectT<int32_t>*>(subj)->get();
}

/* Enciende/apaga AUTO. Al encender se pide una evaluacion ya mismo (la ventana
 * acumulada alcanza: "calibracion inmediata" del issue). Al apagar se vuelve al
 * comportamiento de siempre: ningun parametro se toca mas y el DDS RX vuelve al
 * tono manual (corrimiento 0). Los valores que AUTO dejo quedan en los params:
 * visibles y editables a mano. */
static void on_auto_change(Subject *subj, void *user_data) {
    bool on  = subject_get_int(subj) != 0;
    bool was = cw_decoder_auto.exchange(on);

    if (on && !was) {
        at_request.store(true);
    } else if (!on && was) {
        cw_autotune_reset(&at_state);
        rx_tone_offset = 0.0f;
        dds_dec_init();
    }
}

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "dsp.h"

#include "cw.h"
#include "util.h"
#include "buttons.h"
#include "cfg/subjects.h"

#include <algorithm>
#include <numeric>
#include <mutex>
#include <stddef.h>
#include <float.h>

extern "C" {
    #include "audio.h"
    #include "cfg/cfg.h"
    #include "dialog_msg_voice.h"
    #include "meter.h"
    #include "recorder.h"
    #include "audio_enhancement.h"
    #include "rtty.h"
    #include "spectrum.h"
    #include "waterfall.h"

    #include <math.h>
    #include <stdlib.h>
}


#define ANF_DECIM_FACTOR 8 // 100000 -> 12500
#define ANF_STEP 25 // Hz
#define ANF_NFFT 100000 / ANF_DECIM_FACTOR / ANF_STEP
#define ANF_INTERVAL_MS 500
#define ANF_HIST_LEN 3

template <size_t input_size, size_t output_size> class AveragedPSD {
  private:
    int8_t                         count;
    std::array<float, input_size>  psd;
    std::array<float, output_size> averaged;

    size_t positions[output_size];
    float offsets[output_size];

    std::mutex mutex;

    void lerp_averaged() {
        if (output_size == input_size) {
            for (size_t i = 0; i < output_size; i++) {
                averaged[i] = psd[i];
            }
        } else {
            float a, b;
            for (size_t i = 0; i < output_size; i++) {
                a = psd[positions[i]];
                b = psd[positions[i] + 1];
                averaged[i] = a + (b - a) * offsets[i];
            }
        }
    }

  public:
    AveragedPSD() {
        for (size_t i = 0; i < output_size; i++) {
            float f = (float)i / output_size * input_size;
            positions[i] = f;
            offsets[i] = f - positions[i];
        }
    }

    void reset() {
        const std::lock_guard<std::mutex> lock(mutex);
        count = 0;
        psd.fill(0.0f);
    };

    void add_samples(float *samples) {
        /* BASE 1.0.7/1.0.8 entrega los bins en dB. Se suman en potencia (no
         * en dB) para que el promedio sea el del PSD; GUI upstream v0.1.2,
         * commit c74df4a3. */
        for (size_t i = 0; i < psd.size(); i++) {
            psd[i] += powf(10.0f, samples[i] * 0.1f);
        }
        count++;
    };

    std::array<float, output_size> *get() {
        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (!count) {
                return nullptr;
            }
            liquid_vectorf_mulscalar(psd.data(), psd.size(), 1.0f / count, psd.data());
            lerp_averaged();
        }

        for (size_t i = 0; i < averaged.size(); i++) {
            averaged[i] = 10.0f * log10f(averaged[i] + 1e-12f);
        }

        reset();
        return &averaged;
    };
};

static AveragedPSD<RADIO_SAMPLES, SPECTRUM_NFFT> spectrum_avg_psd;
static AveragedPSD<RADIO_SAMPLES, RADIO_SAMPLES> waterfall_avg_psd;

static uint8_t       spectrum_factor = 1;
static uint32_t      fft_width = FFT_FULL_WIDTH;

static float          spectrum_psd[SPECTRUM_NFFT];
static float          spectrum_psd_filtered[SPECTRUM_NFFT];
static float          spectrum_beta   = 0.7f;
static uint8_t        spectrum_fps_ms = (1000 / 15);
static uint64_t       spectrum_time;
static cfloat         spectrum_dec_buf[SPECTRUM_NFFT / 2];

static float          waterfall_psd[WATERFALL_NFFT];
static uint8_t        waterfall_fps_ms = (1000 / 25);
static uint64_t       waterfall_time;

static cfloat buf_filtered[RADIO_SAMPLES];

static uint32_t cur_freq;
static uint8_t  psd_delay;
static uint8_t  min_max_delay;

static firhilbf audio_hilb;
static cfloat  *audio;

static bool ready = false;
static bool last_tx = false;

static x6200_mode_t cur_mode;

/* --- instantanea espectral para Auto Tune multimodo (issue #205) -----------
 *
 * Un solo mutex guarda la copia publicada, su secuencia, su epoch y el contexto
 * vigente (frecuencia, modo y ancho de FFT). Los observers del GUI escriben el
 * contexto; el hilo del DSP solo lee esos campos para publicar. Se retiene
 * unicamente para copiar ~3.2 KB: nunca para procesar ni para dibujar, y el hilo
 * del DSP no accede a ningun Subject. */
static dsp_autotune_spectrum_t atune_snapshot;
static std::mutex             atune_mutex;
static uint64_t               atune_sequence;
static uint64_t               atune_epoch;
static int32_t                atune_center;
static int32_t                atune_mode;
static int32_t                atune_width;

static void atune_context_set(int32_t *field, int32_t value) {
    const std::lock_guard<std::mutex> lock(atune_mutex);

    if (*field == value) {
        return;
    }
    *field = value;
    /* Un cambio de contexto invalida la publicacion vigente: ninguna trama
     * anterior al cambio puede mezclarse con las posteriores. */
    atune_epoch++;
    atune_snapshot.epoch = atune_epoch;
    atune_snapshot.valid = false;
}

static void dsp_autotune_publish(const float *db, size_t n, bool tx) {
    const std::lock_guard<std::mutex> lock(atune_mutex);
    bool ok = !tx && db != NULL && n == (size_t) SPECTRUM_NFFT && atune_width > 0;

    for (size_t i = 0; i < (size_t) SPECTRUM_NFFT; i++) {
        float v = (i < n && db != NULL) ? db[i] : 0.0f;

        atune_snapshot.db[i] = v;
        if (!isfinite(v)) {
            ok = false;
        }
    }

    atune_sequence++;
    atune_snapshot.sequence = atune_sequence;
    atune_snapshot.timestamp_ms = get_time();
    atune_snapshot.center_freq_hz = atune_center;
    atune_snapshot.mode = atune_mode;
    atune_snapshot.width_hz = atune_width;
    atune_snapshot.tx = tx;
    /* Publicacion invalida (TX, sin datos, ancho no positivo o un valor no
     * finito): avanza su secuencia y queda marcada como no medible. El epoch lo
     * mueve el cambio de contexto (o dsp_autotune_invalidate()), no una trama
     * basura: una sola publicacion mala no tiene que tirar abajo una fase. */
    atune_snapshot.valid = ok;
}

static void dsp_update_min_max(float *data_buf, uint16_t size);
static void on_cur_freq_change(Subject *subj, void *user_data);
static void on_cur_mode_change(Subject *subj, void *user_data);
static void on_cur_fft_width_change(Subject *subj, void *user_data);


/* * */

void dsp_init() {
    spectrum_time  = get_time();
    waterfall_time = get_time();

    psd_delay = 0;

    audio      = (cfloat *)malloc(AUDIO_CAPTURE_RATE * sizeof(cfloat));
    audio_hilb = firhilbf_create(7, 60.0f);

    cfg_cur.fg_freq->subscribe(on_cur_freq_change);
    cfg_cur.mode->subscribe(on_cur_mode_change)->notify();
    /* Ancho de FFT (issue #205): el contexto de la instantanea espectral tiene
     * que llevar el ancho VIGENTE, no una suposicion fija de 100 kHz; con el
     * observer, un zoom durante una medicion se ve como cambio de contexto. */
    cfg_cur.fft_width->subscribe(on_cur_fft_width_change)->notify();
    atune_center = subject_get_int(cfg_cur.fg_freq);
    ready = true;
}

void dsp_reset() {
    waterfall_avg_psd.reset();
    spectrum_avg_psd.reset();
}

static void update_s_meter(int16_t peak_db) {
    if (dialog_msg_voice_get_state() != MSG_VOICE_RECORD) {
        meter_update(peak_db, 0.0f);
    }
}

void dsp_samples(float *buf_samples, uint16_t size, bool tx, int16_t dbm) {
    uint64_t      now = get_time();

    if (last_tx != tx) {
        last_tx = tx;
        spectrum_avg_psd.reset();
        waterfall_avg_psd.reset();
        /* TX/RX: la instantanea vigente deja de valer (issue #205, seccion 4). */
        dsp_autotune_invalidate();
    }

    spectrum_avg_psd.add_samples(buf_samples);
    if (psd_delay) {
        psd_delay--;
    } else {
        waterfall_avg_psd.add_samples(buf_samples);
    }
    if ((now - spectrum_time > spectrum_fps_ms)) {
        auto spectrum_avg_data = spectrum_avg_psd.get();
        if (spectrum_avg_data){
            // Instantanea para el Auto Tune multimodo (issue #205): la MISMA PSD
            // promediada, en dB y ANTES del suavizado visual del waterfall. No
            // hay FFT nueva ni cadencia nueva; el publicador no toca LVGL.
            dsp_autotune_publish(spectrum_avg_data->data(), spectrum_avg_data->size(), tx);
            // Decrease beta for high zoom
            float new_beta = powf(spectrum_beta, ((float)spectrum_factor - 1.0f) / 2.0f + 1.0f);
            lpf_block(spectrum_psd_filtered, spectrum_avg_data->data(), new_beta, SPECTRUM_NFFT);
            spectrum_data(spectrum_psd_filtered, SPECTRUM_NFFT, tx);
            spectrum_time = now;
        }
    }
    if ((now - waterfall_time > waterfall_fps_ms)) {
        auto waterfall_avg_data = waterfall_avg_psd.get();
        if (waterfall_avg_data) {
            waterfall_data(waterfall_avg_data->data(), waterfall_avg_data->size(), tx);
            waterfall_time = now;

            // update meter
            update_s_meter(dbm);

            // update min/max
            if (!tx) {
                // Ignore borders
                dsp_update_min_max(waterfall_avg_data->data() + 8, waterfall_avg_data->size() - 16);
            }
        }
    }
}

static void on_cur_freq_change(Subject *subj, void *user_data) {
    int32_t new_freq = static_cast<SubjectT<int32_t> *>(subj)->get();
    int32_t diff = new_freq - cur_freq;
    cur_freq = new_freq;
    psd_delay = 3;
    waterfall_avg_psd.reset();
    spectrum_avg_psd.reset();
    /* VFO/frecuencia nueva: la instantanea del Auto Tune queda invalidada. */
    atune_context_set(&atune_center, new_freq);
}

static void on_cur_mode_change(Subject *subj, void *user_data) {
    audio_enh_apply_mode_preset();   /* CW/CWR: perfil CW (NR off) */
    cur_mode = (x6200_mode_t)subject_get_int(cfg_cur.mode);
    atune_context_set(&atune_mode, (int32_t) cur_mode);
}

static void on_cur_fft_width_change(Subject *subj, void *user_data) {
    fft_width = (uint32_t) subject_get_int(cfg_cur.fft_width);
    atune_context_set(&atune_width, subject_get_int(cfg_cur.fft_width));
}

bool dsp_autotune_get_spectrum(dsp_autotune_spectrum_t *out) {
    if (out == NULL) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(atune_mutex);

    if (atune_sequence == 0) {
        return false;   /* el DSP todavia no publico ninguna trama */
    }
    *out = atune_snapshot;
    return true;
}

void dsp_autotune_invalidate(void) {
    const std::lock_guard<std::mutex> lock(atune_mutex);

    atune_epoch++;
    atune_snapshot.epoch = atune_epoch;
    atune_snapshot.valid = false;
}

float dsp_get_spectrum_beta() {
    return spectrum_beta;
}

void dsp_set_spectrum_beta(float x) {
    spectrum_beta = x;
}

void dsp_put_audio_samples(size_t nsamples, int16_t *samples) {
    if (!ready) {
        return;
    }

    if (dialog_msg_voice_get_state() == MSG_VOICE_RECORD) {
        dialog_msg_voice_put_audio_samples(nsamples, samples);
        return;
    }

    if (recorder_is_on()) {
        recorder_put_audio_samples(nsamples, samples);
    }

    for (uint16_t i = 0; i < nsamples; i++)
        firhilbf_r2c_execute(audio_hilb, samples[i] / 32768.0f, &audio[i]);

    if (rtty_get_state() == RTTY_RX) {
        rtty_put_audio_samples(nsamples, audio);
    } else if (cur_mode == x6200_mode_cw || cur_mode == x6200_mode_cwr) {
        cw_put_audio_samples(nsamples, audio);
    } else {
        dialog_audio_samples(nsamples, audio);
    }
}

static void dsp_update_min_max(float *data_buf, uint16_t size) {
    if (min_max_delay) {
        min_max_delay--;
        return;
    }

    /* dB a potencia y suma en la ventana de 2.7 kHz: el minimo del PSD suma
     * potencia, no percentil de dB. GUI upstream v0.1.2, commit c74df4a3. */
    float psd[size];
    for (size_t i = 0; i < size; i++) {
        psd[i] = powf(10.0f, data_buf[i] * 0.1f);
    }

    size_t window = (size_t) roundf(2700.0f * RADIO_SAMPLES / (float) fft_width);
    if (window < 1) {
        window = 1;
    }
    if (window >= size) {
        return;
    }

    size_t psd_sum_size = size - window;
    float  psd_sum[psd_sum_size];
    for (size_t i = 0; i < psd_sum_size; i++) {
        psd_sum[i] = 0.0f;
        for (size_t j = 0; j < window; j++) {
            psd_sum[i] += psd[i + j];
        }
    }

    float min = FLT_MAX;
    for (size_t i = 0; i < psd_sum_size; i++) {
        min = LV_MIN(min, psd_sum[i]);
    }

    min = LV_MAX(min, 1e-20f);
    min = 10.0f * log10f(min);

    /* Offset de Minimum Statistics segun el ancho de FFT (upstream). */
    float offset;
    switch (fft_width) {
    case FFT_FULL_WIDTH:
        offset = 14.83f;
        break;
    case FFT_FULL_WIDTH / 2:
        offset = 9.47f;
        break;
    case FFT_FULL_WIDTH / 4:
        offset = 5.57f;
        break;
    default:
        offset = 3.24f;
        break;
    }

    min = min + offset;
    min -= 24.0f;

    float max = min + 48;

    spectrum_update_min(min);
    waterfall_update_min(min);

    spectrum_update_max(max);
    waterfall_update_max(max);
}

/* Accepted for the ported app code; an app is a full dialog over the main
 * screen here, so there is nothing to hide (see dsp.h). */
extern "C" void dsp_set_spectrum_enabled(bool enabled) {
    (void) enabled;
}

extern "C" void dsp_set_waterfall_enabled(bool enabled) {
    (void) enabled;
}

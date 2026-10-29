/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Audio Enhancement Implementation
 *
 *  Copyright (c) 2024 LU2DSX
 */

#include "audio_enhancement.h"
#include "audio.h"              /* AUDIO_CAPTURE_RATE: la tasa REAL del stream capturado */
#include "lvgl/lvgl.h"
#include "cfg/cfg.h"
#include "cfg/subjects.h"
#include "radio.h"
#include "params/params.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <liquid/liquid.h>

/* SpeexDSP se inicializa con la tasa del stream y con tramas de 10 ms. Ambas constantes
 * tienen que seguir a la captura real (mono S16NE, audio.c): si divergen, Speex procesa
 * con una tasa que no es la del audio y el NR deja de tener sentido fisico. Que lo diga
 * el compilador y no un comentario. */
#if AUDIO_ENH_SAMPLE_RATE != AUDIO_CAPTURE_RATE
#error "AUDIO_ENH_SAMPLE_RATE debe ser AUDIO_CAPTURE_RATE (la tasa real de la captura)"
#endif

#if AUDIO_ENH_FRAME_SIZE != (AUDIO_CAPTURE_RATE / 100)
#error "AUDIO_ENH_FRAME_SIZE debe ser una trama de 10 ms de AUDIO_CAPTURE_RATE"
#endif

audio_enh_state_t g_audio_enh = {0};

/* Un unico mutex protege TODO el estado efectivo de la cadena: Speex (run/ctl/destroy),
 * los notches, el nivelador AF y enable/current_preset/NR. audio_enh_process lo mantiene
 * mientras procesa (corre en el hilo del mainloop de PulseAudio) y los setters/getters lo
 * toman cuando acceden. Nunca se mantiene mientras se llaman APIs de LVGL, el mainloop de
 * PulseAudio, x6200_control_*, params_flush ni hooks de UI: por eso audio_enh_process lee
 * el tick antes de tomarlo y audio_enh_apply_mode_preset lee params/Subjects antes.
 *
 * Los helpers con sufijo _locked asumen el mutex tomado: preset_load usa esos para no
 * volver a tomarlo a traves de los setters publicos (mutex no recursivo). */
static pthread_mutex_t g_audio_enh_lock = PTHREAD_MUTEX_INITIALIZER;

/* Descarta el estado pendiente de la maquina NR sin persistir nada: la trama en curso se
 * tira y la trama ya procesada deja de emitirse. El audio viejo nunca pasa al modo nuevo. */
static void nr_frame_reset_locked(void) {
    memset(g_audio_enh.in_frame, 0, sizeof(g_audio_enh.in_frame));
    memset(g_audio_enh.out_frame, 0, sizeof(g_audio_enh.out_frame));
    g_audio_enh.in_count = 0;
    g_audio_enh.out_index = 0;
    g_audio_enh.out_ready = false;
}

static void set_enabled_locked(bool enabled) {
    if (g_audio_enh.enabled != enabled) {
        /* ON<->OFF de la cadena: se descarta la trama pendiente, igual que en el swap de
         * preset. Reencender no puede reemitir audio del modo anterior. */
        nr_frame_reset_locked();
    }

    g_audio_enh.enabled = enabled;
}

static void nr_set_enabled_locked(bool enabled) {
    if (g_audio_enh.nr.enabled != enabled) {
        /* ON<->OFF: se descarta lo pendiente. */
        nr_frame_reset_locked();
    }

    g_audio_enh.nr.enabled = enabled;
}

static void nr_set_level_locked(uint8_t level) {
    if (level > 100) {
        level = 100;
    }

    g_audio_enh.nr.level = level;

    if (g_audio_enh.nr.speex_state) {
        /* API de SpeexDSP: el valor es NEGATIVO y mas negativo = mas atenuacion. El nivel
         * 0-100 del preset se mapea a -10..-30 dB: 0->-10, 40->-18, 60->-22, 100->-30. */
        int noise_suppress = -10 - (int) (((unsigned int) level * 20u) / 100u);

        speex_preprocess_ctl(g_audio_enh.nr.speex_state, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS,
                             &noise_suppress);
    }
}

static void af_leveler_set_enabled_locked(bool enabled) {
    g_audio_enh.af_leveler.enabled = enabled;
}

static void af_leveler_set_mode_locked(af_leveler_mode_t mode) {
    g_audio_enh.af_leveler.mode = mode;

    // Set attack/release rates based on mode
    switch (mode) {
        case AF_LEVELER_FAST:
            g_audio_enh.af_leveler.attack_rate = 0.2f;
            g_audio_enh.af_leveler.release_rate = 0.05f;
            break;
        case AF_LEVELER_MEDIUM:
            g_audio_enh.af_leveler.attack_rate = 0.1f;
            g_audio_enh.af_leveler.release_rate = 0.01f;
            break;
        case AF_LEVELER_SLOW:
            g_audio_enh.af_leveler.attack_rate = 0.05f;
            g_audio_enh.af_leveler.release_rate = 0.005f;
            break;
        default:
            break;
    }
}

static void af_leveler_set_target_locked(float target_db) {
    g_audio_enh.af_leveler.target_db = target_db;
}

static void preset_load_locked(audio_enh_preset_t preset) {
    if (preset >= AUDIO_ENH_PRESET_COUNT) {
        preset = AUDIO_ENH_PRESET_OFF;
    }

    g_audio_enh.current_preset = preset;
    g_audio_enh.preset_modified = false;

    /* Cambiar de preset efectivo reinicia la maquina NR: la trama pendiente de un perfil
     * no se emite en el siguiente (ni siquiera cuando los dos tienen NR). */
    nr_frame_reset_locked();

    switch (preset) {
        case AUDIO_ENH_PRESET_OFF:
            set_enabled_locked(false);
            nr_set_enabled_locked(false);
            af_leveler_set_enabled_locked(false);
            break;

        case AUDIO_ENH_PRESET_CW_WEAK:
            set_enabled_locked(true);
            nr_set_enabled_locked(false);  // NR off to preserve tone
            af_leveler_set_enabled_locked(true);
            af_leveler_set_target_locked(-25.0f);
            af_leveler_set_mode_locked(AF_LEVELER_MEDIUM);
            break;

        case AUDIO_ENH_PRESET_SSB_VOICE:
            set_enabled_locked(true);
            nr_set_enabled_locked(true);
            nr_set_level_locked(40);
            af_leveler_set_enabled_locked(false);  // Use hardware AGC
            break;

        case AUDIO_ENH_PRESET_AM_BROADCAST:
            set_enabled_locked(true);
            nr_set_enabled_locked(true);
            nr_set_level_locked(60);
            af_leveler_set_enabled_locked(true);
            af_leveler_set_target_locked(-18.0f);
            af_leveler_set_mode_locked(AF_LEVELER_SLOW);
            break;

        case AUDIO_ENH_PRESET_DIGITAL:
            set_enabled_locked(false);  // Clean audio for digital
            nr_set_enabled_locked(false);
            af_leveler_set_enabled_locked(false);
            break;

        default:
            /* Out-of-range values were already clamped to OFF above. */
            break;
    }
}

/* Libera notches y Speex con el mutex tomado. Lo usan audio_enh_deinit() y audio_enh_init()
 * (este ultimo para poder re-inicializar sin filtrar ni duplicar estados). */
static void deinit_locked(void) {
    // Clean up notch filters
    for (int i = 0; i < AUDIO_ENH_MAX_NOTCHES; i++) {
        if (g_audio_enh.notches[i].filter_obj) {
            firfilt_rrrf_destroy((firfilt_rrrf) g_audio_enh.notches[i].filter_obj);
            g_audio_enh.notches[i].filter_obj = NULL;
        }
        g_audio_enh.notches[i].active = false;
    }

    // Clean up SpeexDSP
    if (g_audio_enh.nr.speex_state) {
        speex_preprocess_state_destroy(g_audio_enh.nr.speex_state);
        g_audio_enh.nr.speex_state = NULL;
    }

    nr_frame_reset_locked();
}

void audio_enh_init(void) {
    pthread_mutex_lock(&g_audio_enh_lock);

    /* Idempotente: si ya habia estado (re-init de una corrida de tests o rearme despues de
     * un deinit), se libera antes de reconstruirlo. */
    deinit_locked();
    memset(&g_audio_enh, 0, sizeof(g_audio_enh));

    // Initialize SpeexDSP
    g_audio_enh.nr.speex_state = speex_preprocess_state_init(AUDIO_ENH_FRAME_SIZE,
                                                             AUDIO_ENH_SAMPLE_RATE);
    if (g_audio_enh.nr.speex_state) {
        int denoise_enabled = 1;

        speex_preprocess_ctl(g_audio_enh.nr.speex_state, SPEEX_PREPROCESS_SET_DENOISE,
                             &denoise_enabled);
    }

    // Initialize AF Leveler with defaults
    g_audio_enh.af_leveler.target_db = -25.0f;
    g_audio_enh.af_leveler.mode = AF_LEVELER_MEDIUM;
    g_audio_enh.af_leveler.attack_rate = 0.1f;
    g_audio_enh.af_leveler.release_rate = 0.01f;

    // Default to OFF preset
    preset_load_locked(AUDIO_ENH_PRESET_OFF);

    pthread_mutex_unlock(&g_audio_enh_lock);
}

void audio_enh_deinit(void) {
    pthread_mutex_lock(&g_audio_enh_lock);
    deinit_locked();
    pthread_mutex_unlock(&g_audio_enh_lock);
}

void audio_enh_set_enabled(bool enabled) {
    pthread_mutex_lock(&g_audio_enh_lock);
    set_enabled_locked(enabled);
    pthread_mutex_unlock(&g_audio_enh_lock);
}

bool audio_enh_is_enabled(void) {
    pthread_mutex_lock(&g_audio_enh_lock);
    bool enabled = g_audio_enh.enabled;
    pthread_mutex_unlock(&g_audio_enh_lock);

    return enabled;
}

int audio_enh_notch_add(float freq_hz, float bandwidth_hz) {
    int slot = -1;

    pthread_mutex_lock(&g_audio_enh_lock);

    // Find first inactive slot
    for (int i = 0; i < AUDIO_ENH_MAX_NOTCHES; i++) {
        if (!g_audio_enh.notches[i].active) {
            // Create FIR notch filter using liquid-dsp
            //   _m  : semi-length (filter length = 2*m+1), larger = sharper notch
            //   _As : stop-band attenuation in dB
            //   _f0 : normalized center frequency in [-0.5, 0.5]
            unsigned int m = 32;         // semi-length for sharp notch
            float As = 60.0f;            // 60 dB stop-band attenuation
            float f0 = freq_hz / (float) AUDIO_ENH_SAMPLE_RATE;

            firfilt_rrrf filter = firfilt_rrrf_create_notch(m, As, f0);

            if (!filter) {
                break;
            }

            g_audio_enh.notches[i].active = true;
            g_audio_enh.notches[i].freq_hz = freq_hz;
            g_audio_enh.notches[i].bandwidth_hz = bandwidth_hz;
            g_audio_enh.notches[i].filter_obj = filter;
            slot = i;
            break;
        }
    }

    pthread_mutex_unlock(&g_audio_enh_lock);

    return slot;  // -1: no slots available (or allocation failed)
}

static void notch_remove_locked(int notch_id) {
    if (notch_id < 0 || notch_id >= AUDIO_ENH_MAX_NOTCHES) return;
    if (!g_audio_enh.notches[notch_id].active) return;

    if (g_audio_enh.notches[notch_id].filter_obj) {
        firfilt_rrrf_destroy((firfilt_rrrf) g_audio_enh.notches[notch_id].filter_obj);
    }

    g_audio_enh.notches[notch_id].active = false;
    g_audio_enh.notches[notch_id].filter_obj = NULL;
}

void audio_enh_notch_remove(int notch_id) {
    pthread_mutex_lock(&g_audio_enh_lock);
    notch_remove_locked(notch_id);
    pthread_mutex_unlock(&g_audio_enh_lock);
}

void audio_enh_notch_clear_all(void) {
    pthread_mutex_lock(&g_audio_enh_lock);

    for (int i = 0; i < AUDIO_ENH_MAX_NOTCHES; i++) {
        notch_remove_locked(i);
    }

    pthread_mutex_unlock(&g_audio_enh_lock);
}

void audio_enh_af_leveler_set_enabled(bool enabled) {
    pthread_mutex_lock(&g_audio_enh_lock);
    af_leveler_set_enabled_locked(enabled);
    pthread_mutex_unlock(&g_audio_enh_lock);
}

void audio_enh_af_leveler_set_mode(af_leveler_mode_t mode) {
    pthread_mutex_lock(&g_audio_enh_lock);
    af_leveler_set_mode_locked(mode);
    pthread_mutex_unlock(&g_audio_enh_lock);
}

void audio_enh_af_leveler_set_target(float target_db) {
    pthread_mutex_lock(&g_audio_enh_lock);
    af_leveler_set_target_locked(target_db);
    pthread_mutex_unlock(&g_audio_enh_lock);
}

float audio_enh_af_leveler_get_gain_reduction(void) {
    pthread_mutex_lock(&g_audio_enh_lock);
    float gain_db = g_audio_enh.af_leveler.current_gain_db;
    pthread_mutex_unlock(&g_audio_enh_lock);

    return gain_db;
}

float audio_enh_af_leveler_get_gain_reduction_peak(void) {
    pthread_mutex_lock(&g_audio_enh_lock);
    float gain_db = g_audio_enh.af_leveler.peak_gain_db;
    pthread_mutex_unlock(&g_audio_enh_lock);

    return gain_db;
}

void audio_enh_nr_set_enabled(bool enabled) {
    pthread_mutex_lock(&g_audio_enh_lock);
    nr_set_enabled_locked(enabled);
    pthread_mutex_unlock(&g_audio_enh_lock);
}

void audio_enh_nr_set_level(uint8_t level) {
    pthread_mutex_lock(&g_audio_enh_lock);
    nr_set_level_locked(level);
    pthread_mutex_unlock(&g_audio_enh_lock);
}

bool audio_enh_nr_is_enabled(void) {
    pthread_mutex_lock(&g_audio_enh_lock);
    bool enabled = g_audio_enh.nr.enabled;
    pthread_mutex_unlock(&g_audio_enh_lock);

    return enabled;
}

void audio_enh_nr_reset_pending(void) {
    pthread_mutex_lock(&g_audio_enh_lock);
    nr_frame_reset_locked();
    pthread_mutex_unlock(&g_audio_enh_lock);
}

void audio_enh_preset_load(audio_enh_preset_t preset) {
    pthread_mutex_lock(&g_audio_enh_lock);
    preset_load_locked(preset);
    pthread_mutex_unlock(&g_audio_enh_lock);
}

void audio_enh_preset_reset(void) {
    audio_enh_preset_load(audio_enh_preset_get_current());
}

audio_enh_preset_t audio_enh_preset_get_current(void) {
    pthread_mutex_lock(&g_audio_enh_lock);
    audio_enh_preset_t preset = g_audio_enh.current_preset;
    pthread_mutex_unlock(&g_audio_enh_lock);

    return preset;
}

const char *audio_enh_preset_get_name(audio_enh_preset_t preset) {
    switch (preset) {
        case AUDIO_ENH_PRESET_OFF: return "OFF";
        case AUDIO_ENH_PRESET_CW_WEAK: return "CW Weak";
        case AUDIO_ENH_PRESET_SSB_VOICE: return "SSB Voice";
        case AUDIO_ENH_PRESET_AM_BROADCAST: return "AM Broadcast";
        case AUDIO_ENH_PRESET_DIGITAL: return "Digital";
        default: return "Unknown";
    }
}

static void apply_notch_filters_locked(float *samples, size_t nsamples) {
    for (int n = 0; n < AUDIO_ENH_MAX_NOTCHES; n++) {
        if (g_audio_enh.notches[n].active && g_audio_enh.notches[n].filter_obj) {
            firfilt_rrrf notch = (firfilt_rrrf)g_audio_enh.notches[n].filter_obj;
            float y;
            for (size_t i = 0; i < nsamples; i++) {
                firfilt_rrrf_push(notch, samples[i]);
                firfilt_rrrf_execute(notch, &y);
                samples[i] = y;
            }
        }
    }
}

static void apply_af_leveler_locked(float *samples, size_t nsamples, uint32_t now) {
    if (!g_audio_enh.af_leveler.enabled || nsamples == 0) return;

    af_leveler_t *agc = &g_audio_enh.af_leveler;

    /* Level measured once per block (not per sample): one log10f + one powf per
     * block keeps the leveler affordable inside the audio callback, which
     * matters for battery life on the R16. */
    float peak = 0.0f;

    for (size_t i = 0; i < nsamples; i++) {
        float a = fabsf(samples[i]);

        if (a > peak) {
            peak = a;
        }
    }

    float input_db = 20.0f * log10f(peak + 1e-6f);
    float desired_gain = agc->target_db - input_db;

    if (desired_gain < agc->current_gain_db) {
        agc->current_gain_db += agc->attack_rate * (desired_gain - agc->current_gain_db);
    } else {
        agc->current_gain_db += agc->release_rate * (desired_gain - agc->current_gain_db);
    }

    /* Limit gain range */
    if (agc->current_gain_db > 30.0f) agc->current_gain_db = 30.0f;
    if (agc->current_gain_db < -30.0f) agc->current_gain_db = -30.0f;

    float gain_linear = powf(10.0f, agc->current_gain_db / 20.0f);

    for (size_t i = 0; i < nsamples; i++) {
        samples[i] *= gain_linear;
    }

    // Peak hold
    static uint32_t last_peak_update = 0;
    if (agc->current_gain_db < agc->peak_gain_db) {
        agc->peak_gain_db = agc->current_gain_db;
        last_peak_update = now;
    } else if ((now - last_peak_update) > 1000) {
        agc->peak_gain_db = agc->current_gain_db;
    }
}

static int16_t s16_from_float(float s) {
    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;

    float scaled = s * 32768.0f;

    if (scaled > 32767.0f) scaled = 32767.0f;
    if (scaled < -32768.0f) scaled = -32768.0f;

    return (int16_t) scaled;
}

/* Maquina NR: N muestras de salida por N de entrada, tramas completas de
 * AUDIO_ENH_FRAME_SIZE y sin cola cruda por callback.
 *
 * Por cada muestra PRE-NR (despues de notch y nivelador AF) se hace, en este orden:
 *   1. se emite la siguiente muestra de la trama lista anterior, o 0 mientras la primera
 *      trama aun no existe (priming de exactamente UNA trama: 441 muestras / 10 ms);
 *   2. la muestra actual entra en in_frame;
 *   3. al juntar AUDIO_ENH_FRAME_SIZE se corre UNA vez speex_preprocess_run() y esa trama
 *      pasa a ser la siguiente out_frame, que recien empieza a emitirse en la muestra
 *      siguiente.
 * El resultado es un retardo digital constante de una trama y ninguna muestra perdida: los
 * limites entre callbacks (o entre bloques de la particion de la captura) no cambian la
 * secuencia.
 * Sin NR (Off/Digital no llegan aca porque la cadena esta deshabilitada; CW weak si) o sin
 * estado Speex se pasa el audio PRE-NR tal cual, sin priming: nunca silencio permanente. */
static void apply_noise_reduction_locked(const float *in, int16_t *out, size_t nsamples) {
    if (!g_audio_enh.nr.enabled || g_audio_enh.nr.speex_state == NULL) {
        for (size_t i = 0; i < nsamples; i++) {
            out[i] = s16_from_float(in[i]);
        }

        return;
    }

    for (size_t i = 0; i < nsamples; i++) {
        int16_t emitted = 0;

        if (g_audio_enh.out_ready) {
            emitted = g_audio_enh.out_frame[g_audio_enh.out_index];
            g_audio_enh.out_index++;

            if (g_audio_enh.out_index >= AUDIO_ENH_FRAME_SIZE) {
                g_audio_enh.out_index = 0;
                g_audio_enh.out_ready = false;
            }
        }

        out[i] = emitted;

        g_audio_enh.in_frame[g_audio_enh.in_count] = s16_from_float(in[i]);
        g_audio_enh.in_count++;

        if (g_audio_enh.in_count >= AUDIO_ENH_FRAME_SIZE) {
            speex_preprocess_run(g_audio_enh.nr.speex_state, g_audio_enh.in_frame);
            memcpy(g_audio_enh.out_frame, g_audio_enh.in_frame,
                   sizeof(g_audio_enh.out_frame));
            g_audio_enh.in_count = 0;
            g_audio_enh.out_index = 0;
            g_audio_enh.out_ready = true;
        }
    }
}

/* En CW/CWR el preset de voz (NR Speex + nivelador AF) bombea y enmascara el tono: el
 * decodificador pierde pulsos y el Recorder, que graba DESPUES de este procesamiento
 * (audio.c: audio_enh_process() antes de dsp_put_audio_samples()), guardaba audio
 * entrecortado. Medido en la radio con preset SSB_VOICE en modo CW: captura continua
 * (parec 441000 B en 5 s exactos, sin clicks ni silencio) pero envolvente con 12.9% de
 * bloques por debajo de la mitad de la mediana.
 *
 * Se usa el perfil CW (NR off para preservar el tono) sin tocar la preferencia guardada:
 * en SSB/AM/etc. sigue aplicando el preset que eligio el usuario. OFF y DIGITAL son
 * intenciones explicitas de no procesar y se respetan tambien en CW. */
void audio_enh_apply_mode_preset(void) {
    /* params y Subjects se leen ANTES de tomar el mutex del modulo: son superficies de la
     * GUI/radio y no se tocan con el lock de la cadena tomado. */
    audio_enh_preset_t user = (audio_enh_preset_t) params.audio_enh_preset.x;
    x6200_mode_t       mode = (x6200_mode_t) subject_get_int(cfg_cur.mode);
    audio_enh_preset_t effective = user;

    if (mode == x6200_mode_cw || mode == x6200_mode_cwr) {
        if (user != AUDIO_ENH_PRESET_OFF && user != AUDIO_ENH_PRESET_DIGITAL) {
            effective = AUDIO_ENH_PRESET_CW_WEAK;
        }
    }

    if (effective != audio_enh_preset_get_current()) {
        audio_enh_preset_load(effective);
    }
}

void audio_enh_process(int16_t *samples, size_t nsamples) {
    if (samples == NULL || nsamples == 0) return;

    /* lv_tick_get() es una API de LVGL: se lee FUERA del mutex del modulo (nunca se
     * mantiene el lock de la cadena mientras se llama a LVGL). */
    uint32_t now = lv_tick_get();

    pthread_mutex_lock(&g_audio_enh_lock);

    if (!g_audio_enh.enabled) {
        /* Off/Digital: passthrough bit-exacto, sin conversion ni priming. */
        pthread_mutex_unlock(&g_audio_enh_lock);
        return;
    }

    /* La captura entrega ~100 ms por callback, pero el contrato no depende de eso: se
     * procesa en bloques de una trama y la maquina NR sigue del bloque anterior. */
    size_t done = 0;

    while (done < nsamples) {
        size_t chunk = nsamples - done;

        if (chunk > AUDIO_ENH_FRAME_SIZE) {
            chunk = AUDIO_ENH_FRAME_SIZE;
        }

        // Convert int16 to float
        for (size_t i = 0; i < chunk; i++) {
            g_audio_enh.float_buffer[i] = samples[done + i] / 32768.0f;
        }

        // Apply processing chain: notch -> AF leveler -> Speex NR
        apply_notch_filters_locked(g_audio_enh.float_buffer, chunk);
        apply_af_leveler_locked(g_audio_enh.float_buffer, chunk, now);
        apply_noise_reduction_locked(g_audio_enh.float_buffer, &samples[done], chunk);

        done += chunk;
    }

    pthread_mutex_unlock(&g_audio_enh_lock);
}

bool audio_enh_notch_is_active(int notch_id) {
    if (notch_id < 0 || notch_id >= AUDIO_ENH_MAX_NOTCHES) return false;

    pthread_mutex_lock(&g_audio_enh_lock);
    bool active = g_audio_enh.notches[notch_id].active;
    pthread_mutex_unlock(&g_audio_enh_lock);

    return active;
}

int audio_enh_get_active_notch_count(void) {
    int count = 0;

    pthread_mutex_lock(&g_audio_enh_lock);

    for (int i = 0; i < AUDIO_ENH_MAX_NOTCHES; i++) {
        if (g_audio_enh.notches[i].active) count++;
    }

    pthread_mutex_unlock(&g_audio_enh_lock);

    return count;
}

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Audio Enhancement Implementation
 *
 *  Copyright (c) 2024 LU2DSX
 */

#include "audio_enhancement.h"
#include "lvgl/lvgl.h"
#include "cfg/cfg.h"
#include "cfg/subjects.h"
#include "radio.h"
#include "params/params.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <liquid/liquid.h>

audio_enh_state_t g_audio_enh = {0};

void audio_enh_init(void) {
    memset(&g_audio_enh, 0, sizeof(g_audio_enh));
    
    // Initialize SpeexDSP
    g_audio_enh.nr.speex_state = speex_preprocess_state_init(AUDIO_ENH_FRAME_SIZE, AUDIO_ENH_SAMPLE_RATE);
    if (g_audio_enh.nr.speex_state) {
        int denoise_enabled = 1;
        speex_preprocess_ctl(g_audio_enh.nr.speex_state, SPEEX_PREPROCESS_SET_DENOISE, &denoise_enabled);
    }
    
    // Initialize AF Leveler with defaults
    g_audio_enh.af_leveler.target_db = -25.0f;
    g_audio_enh.af_leveler.mode = AF_LEVELER_MEDIUM;
    g_audio_enh.af_leveler.attack_rate = 0.1f;
    g_audio_enh.af_leveler.release_rate = 0.01f;
    
    // Default to OFF preset
    audio_enh_preset_load(AUDIO_ENH_PRESET_OFF);
}

void audio_enh_deinit(void) {
    // Clean up notch filters
    for (int i = 0; i < AUDIO_ENH_MAX_NOTCHES; i++) {
        if (g_audio_enh.notches[i].active && g_audio_enh.notches[i].filter_obj) {
            firfilt_rrrf_destroy((firfilt_rrrf)g_audio_enh.notches[i].filter_obj);
        }
    }
    
    // Clean up SpeexDSP
    if (g_audio_enh.nr.speex_state) {
        speex_preprocess_state_destroy(g_audio_enh.nr.speex_state);
        g_audio_enh.nr.speex_state = NULL;
    }
}

void audio_enh_set_enabled(bool enabled) {
    g_audio_enh.enabled = enabled;
}

bool audio_enh_is_enabled(void) {
    return g_audio_enh.enabled;
}

int audio_enh_notch_add(float freq_hz, float bandwidth_hz) {
    // Find first inactive slot
    for (int i = 0; i < AUDIO_ENH_MAX_NOTCHES; i++) {
        if (!g_audio_enh.notches[i].active) {
            // Create FIR notch filter using liquid-dsp
            //   _m  : semi-length (filter length = 2*m+1), larger = sharper notch
            //   _As : stop-band attenuation in dB
            //   _f0 : normalized center frequency in [-0.5, 0.5]
            unsigned int m = 32;         // semi-length for sharp notch
            float As = 60.0f;            // 60 dB stop-band attenuation
            float f0 = freq_hz / (float)AUDIO_ENH_SAMPLE_RATE;

            firfilt_rrrf filter = firfilt_rrrf_create_notch(m, As, f0);
            if (!filter) return -1;

            g_audio_enh.notches[i].active = true;
            g_audio_enh.notches[i].freq_hz = freq_hz;
            g_audio_enh.notches[i].bandwidth_hz = bandwidth_hz;
            g_audio_enh.notches[i].filter_obj = filter;

            return i;
        }
    }
    return -1;  // No slots available
}

void audio_enh_notch_remove(int notch_id) {
    if (notch_id < 0 || notch_id >= AUDIO_ENH_MAX_NOTCHES) return;
    if (!g_audio_enh.notches[notch_id].active) return;
    
    if (g_audio_enh.notches[notch_id].filter_obj) {
        firfilt_rrrf_destroy((firfilt_rrrf)g_audio_enh.notches[notch_id].filter_obj);
    }
    
    g_audio_enh.notches[notch_id].active = false;
    g_audio_enh.notches[notch_id].filter_obj = NULL;
}

void audio_enh_notch_clear_all(void) {
    for (int i = 0; i < AUDIO_ENH_MAX_NOTCHES; i++) {
        audio_enh_notch_remove(i);
    }
}

void audio_enh_af_leveler_set_enabled(bool enabled) {
    g_audio_enh.af_leveler.enabled = enabled;
}

void audio_enh_af_leveler_set_mode(af_leveler_mode_t mode) {
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

void audio_enh_af_leveler_set_target(float target_db) {
    g_audio_enh.af_leveler.target_db = target_db;
}

float audio_enh_af_leveler_get_gain_reduction(void) {
    return g_audio_enh.af_leveler.current_gain_db;
}

float audio_enh_af_leveler_get_gain_reduction_peak(void) {
    return g_audio_enh.af_leveler.peak_gain_db;
}

void audio_enh_nr_set_enabled(bool enabled) {
    g_audio_enh.nr.enabled = enabled;
}

void audio_enh_nr_set_level(uint8_t level) {
    g_audio_enh.nr.level = level;
    
    if (g_audio_enh.nr.speex_state) {
        // Map 0-100 to Speex noise suppression level (typically -30 to -10 dB)
        int noise_suppress = -30 + (level * 20 / 100);
        speex_preprocess_ctl(g_audio_enh.nr.speex_state, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress);
    }
}

bool audio_enh_nr_is_enabled(void) {
    return g_audio_enh.nr.enabled;
}

void audio_enh_preset_load(audio_enh_preset_t preset) {
    if (preset >= AUDIO_ENH_PRESET_COUNT) {
        preset = AUDIO_ENH_PRESET_OFF;
    }

    g_audio_enh.current_preset = preset;
    g_audio_enh.preset_modified = false;
    
    switch (preset) {
        case AUDIO_ENH_PRESET_OFF:
            audio_enh_set_enabled(false);
            audio_enh_nr_set_enabled(false);
            audio_enh_af_leveler_set_enabled(false);
            break;
            
        case AUDIO_ENH_PRESET_CW_WEAK:
            audio_enh_set_enabled(true);
            audio_enh_nr_set_enabled(false);  // NR off to preserve tone
            audio_enh_af_leveler_set_enabled(true);
            audio_enh_af_leveler_set_target(-25.0f);
            audio_enh_af_leveler_set_mode(AF_LEVELER_MEDIUM);
            break;
            
        case AUDIO_ENH_PRESET_SSB_VOICE:
            audio_enh_set_enabled(true);
            audio_enh_nr_set_enabled(true);
            audio_enh_nr_set_level(40);
            audio_enh_af_leveler_set_enabled(false);  // Use hardware AGC
            break;
            
        case AUDIO_ENH_PRESET_AM_BROADCAST:
            audio_enh_set_enabled(true);
            audio_enh_nr_set_enabled(true);
            audio_enh_nr_set_level(60);
            audio_enh_af_leveler_set_enabled(true);
            audio_enh_af_leveler_set_target(-18.0f);
            audio_enh_af_leveler_set_mode(AF_LEVELER_SLOW);
            break;
            
        case AUDIO_ENH_PRESET_DIGITAL:
            audio_enh_set_enabled(false);  // Clean audio for digital
            audio_enh_nr_set_enabled(false);
            audio_enh_af_leveler_set_enabled(false);
            break;

        default:
            /* Out-of-range values were already clamped to OFF above. */
            break;
    }
}

audio_enh_preset_t audio_enh_preset_get_current(void) {
    return g_audio_enh.current_preset;
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

static void apply_notch_filters(float *samples, size_t nsamples) {
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

static void apply_af_leveler(float *samples, size_t nsamples) {
    if (!g_audio_enh.af_leveler.enabled || nsamples == 0) return;

    af_leveler_t *agc = &g_audio_enh.af_leveler;
    uint32_t now = lv_tick_get();

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

static void apply_noise_reduction(float *samples, size_t nsamples) {
    if (!g_audio_enh.nr.enabled || !g_audio_enh.nr.speex_state) return;

    /* SpeexDSP runs on fixed 256-sample frames, so only complete frames are
     * processed; a partial tail (up to 255 samples) is passed through. */
    size_t n = nsamples - (nsamples % AUDIO_ENH_FRAME_SIZE);

    for (size_t off = 0; off < n; off += AUDIO_ENH_FRAME_SIZE) {
        int16_t temp_buffer[AUDIO_ENH_FRAME_SIZE];

        for (size_t i = 0; i < AUDIO_ENH_FRAME_SIZE; i++) {
            float s = samples[off + i];

            if (s > 1.0f) s = 1.0f;
            if (s < -1.0f) s = -1.0f;

            temp_buffer[i] = (int16_t)(s * 32767.0f);
        }

        speex_preprocess_run(g_audio_enh.nr.speex_state, temp_buffer);

        for (size_t i = 0; i < AUDIO_ENH_FRAME_SIZE; i++) {
            samples[off + i] = temp_buffer[i] / 32767.0f;
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
    audio_enh_preset_t user = (audio_enh_preset_t) params.audio_enh_preset.x;
    x6200_mode_t       mode = (x6200_mode_t) subject_get_int(cfg_cur.mode);
    audio_enh_preset_t effective = user;

    if (mode == x6200_mode_cw || mode == x6200_mode_cwr) {
        if (user != AUDIO_ENH_PRESET_OFF && user != AUDIO_ENH_PRESET_DIGITAL) {
            effective = AUDIO_ENH_PRESET_CW_WEAK;
        }
    }

    if (effective != g_audio_enh.current_preset) {
        audio_enh_preset_load(effective);
    }
}

void audio_enh_process(int16_t *samples, size_t nsamples) {
    if (!g_audio_enh.enabled) return;
    if (samples == NULL || nsamples == 0) return;

    /* The capture callback delivers ~100 ms blocks (44100 Hz -> ~4410 samples),
     * so the chain is applied in AUDIO_ENH_FRAME_SIZE chunks instead of
     * rejecting anything larger than one frame. */
    while (nsamples > 0) {
        size_t chunk = (nsamples > AUDIO_ENH_FRAME_SIZE) ? AUDIO_ENH_FRAME_SIZE : nsamples;

        // Convert int16 to float
        for (size_t i = 0; i < chunk; i++) {
            g_audio_enh.float_buffer[i] = samples[i] / 32768.0f;
        }

        // Apply processing chain
        apply_notch_filters(g_audio_enh.float_buffer, chunk);
        apply_af_leveler(g_audio_enh.float_buffer, chunk);
        apply_noise_reduction(g_audio_enh.float_buffer, chunk);

        // Convert back to int16
        for (size_t i = 0; i < chunk; i++) {
            float sample = g_audio_enh.float_buffer[i] * 32768.0f;

            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;

            samples[i] = (int16_t)sample;
        }

        samples += chunk;
        nsamples -= chunk;
    }
}

bool audio_enh_notch_is_active(int notch_id) {
    if (notch_id < 0 || notch_id >= AUDIO_ENH_MAX_NOTCHES) return false;
    return g_audio_enh.notches[notch_id].active;
}

int audio_enh_get_active_notch_count(void) {
    int count = 0;
    for (int i = 0; i < AUDIO_ENH_MAX_NOTCHES; i++) {
        if (g_audio_enh.notches[i].active) count++;
    }
    return count;
}

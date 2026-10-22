/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Audio Enhancement
 *
 *  Copyright (c) 2024 LU2DSX
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <speex/speex_preprocess.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_ENH_MAX_NOTCHES 4
#define AUDIO_ENH_SAMPLE_RATE 48000
#define AUDIO_ENH_FRAME_SIZE 256

/* AF Leveler (Software AGC) modes */
typedef enum {
    AF_LEVELER_OFF = 0,
    AF_LEVELER_FAST,
    AF_LEVELER_MEDIUM,
    AF_LEVELER_SLOW
} af_leveler_mode_t;

/* Notch filter state */
typedef struct {
    bool active;
    float freq_hz;
    float bandwidth_hz;
    void *filter_obj;  // liquid-dsp iirnotch object
} notch_filter_t;

/* AF Leveler state */
typedef struct {
    bool enabled;
    af_leveler_mode_t mode;
    float target_db;
    float current_gain_db;
    float peak_gain_db;
    float attack_rate;
    float release_rate;
    uint32_t peak_hold_time;
} af_leveler_t;

/* Noise reduction state */
typedef struct {
    bool enabled;
    uint8_t level;  // 0-100
    SpeexPreprocessState *speex_state;
    float noise_gate_thresh;
} nr_state_t;

/* Mode presets */
typedef enum {
    AUDIO_ENH_PRESET_OFF = 0,
    AUDIO_ENH_PRESET_CW_WEAK,
    AUDIO_ENH_PRESET_SSB_VOICE,
    AUDIO_ENH_PRESET_AM_BROADCAST,
    AUDIO_ENH_PRESET_DIGITAL,
    AUDIO_ENH_PRESET_COUNT
} audio_enh_preset_t;

/* Master state */
typedef struct {
    bool enabled;
    notch_filter_t notches[AUDIO_ENH_MAX_NOTCHES];
    af_leveler_t af_leveler;
    nr_state_t nr;
    audio_enh_preset_t current_preset;
    bool preset_modified;
    
    // Processing buffer
    float float_buffer[AUDIO_ENH_FRAME_SIZE];
} audio_enh_state_t;

/* Global state */
extern audio_enh_state_t g_audio_enh;

/* Lifecycle */
void audio_enh_init(void);
void audio_enh_deinit(void);
void audio_enh_process(int16_t *samples, size_t nsamples);

/* Enable/disable */
void audio_enh_set_enabled(bool enabled);
bool audio_enh_is_enabled(void);

/* Notch filter control */
int audio_enh_notch_add(float freq_hz, float bandwidth_hz);
void audio_enh_notch_remove(int notch_id);
void audio_enh_notch_clear_all(void);
void audio_enh_notch_set_freq(int notch_id, float freq_hz);
void audio_enh_notch_set_bw(int notch_id, float bandwidth_hz);

/* AF Leveler control */
void audio_enh_af_leveler_set_enabled(bool enabled);
void audio_enh_af_leveler_set_mode(af_leveler_mode_t mode);
void audio_enh_af_leveler_set_target(float target_db);
float audio_enh_af_leveler_get_gain_reduction(void);
float audio_enh_af_leveler_get_gain_reduction_peak(void);

/* Noise reduction control */
void audio_enh_nr_set_enabled(bool enabled);
void audio_enh_nr_set_level(uint8_t level);
bool audio_enh_nr_is_enabled(void);

/* Presets */
void audio_enh_preset_load(audio_enh_preset_t preset);

/* Recalcula el perfil EFECTIVO según el modo: en CW/CWR se usa el perfil CW (NR off) salvo
 * que el preset guardado sea OFF o DIGITAL. La preferencia del usuario (params.audio_enh_preset)
 * no se modifica: sólo cambia el procesamiento. Llamar al arrancar, al cambiar de modo y al
 * cambiar el preset en Settings. */
void audio_enh_apply_mode_preset(void);
void audio_enh_preset_reset(void);
audio_enh_preset_t audio_enh_preset_get_current(void);
const char *audio_enh_preset_get_name(audio_enh_preset_t preset);

/* Status */
bool audio_enh_notch_is_active(int notch_id);
int audio_enh_get_active_notch_count(void);

#ifdef __cplusplus
}
#endif

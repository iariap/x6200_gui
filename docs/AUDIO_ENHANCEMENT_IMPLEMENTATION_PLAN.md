# Audio Enhancement Implementation Plan
## X6200 QRP Ham Radio Firmware

**Version:** 1.0  
**Date:** February 2026  
**Author:** LU2DSX  
**Status:** Ready for Implementation

---

## Executive Summary

This document outlines the implementation of professional-grade software audio filtering for the X6200, significantly improving weak signal copy and QRM rejection beyond hardware capabilities. The implementation follows QRP principles: battery-efficient, lightweight, and field-friendly.

### Key Features
- **4 Software Notch Filters** (in addition to hardware DNF)
- **Noise Reduction** (SpeexDSP with basic fallback)
- **AF Leveler** (software AGC with visual feedback)
- **Mode-Specific Presets** (CW, SSB, AM, Digital)
- **Runtime Capability Detection** (adapts to available libraries)

### Power Impact
- **Disabled:** 0% overhead
- **Enabled:** +10-15% CPU (+5-8% battery)
- **Recommendation:** Disable for long battery-only operations

---

## 1. Architecture Overview

### 1.1 Audio Processing Chain

```
Radio Hardware DSP
       ↓
PulseAudio Capture (48kHz, int16)
       ↓
┌──────────────────────────────────────────┐
│   Audio Enhancement Module (NEW)         │
│  ┌──────────────┐ ┌──────────────┐      │
│  │ Notch Filters│→│ AF Leveler   │→     │
│  │ (4x liquid)  │ │ (gain ctrl)  │      │
│  └──────────────┘ └──────────────┘      │
│         ↓                                │
│  ┌──────────────────────────┐           │
│  │ Noise Reduction          │           │
│  │ SpeexDSP OR Basic NR     │           │
│  │ (gate/eq/expand)         │           │
│  └──────────────────────────┘           │
└──────────────────────────────────────────┘
       ↓
Audio Output (Headphones/Speaker)
       ↓
DSP (Waterfall/Spectrum)
```

### 1.2 Module Responsibilities

| Module | Function | Library |
|--------|----------|---------|
| **Notch Filters** | Remove interfering carriers | liquid-dsp (iirnotch) |
| **AF Leveler** | Smooth gain control | Custom (lightweight) |
| **NR Core** | Noise suppression | SpeexDSP OR Basic fallback |
| **Preset Manager** | Mode-optimized settings | None (internal) |
| **CPU Monitor** | Track processing load | /proc/stat |

---

## 2. Implementation Phases

### Phase 1: Core Module Foundation
**Duration:** 2-3 days  
**Files:** `audio_enhancement.h`, `audio_enhancement.c`

#### 2.1.1 Header Design (`audio_enhancement.h`)

```c
/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 Audio Enhancement Module
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Capability detection - runtime */
typedef struct {
    bool has_speex;          // SpeexDSP available
    bool has_liquid_fft;     // liquid-dsp FFT available
    bool has_pro_notch;      // Advanced notch features
} audio_enh_caps_t;

/* Notch bandwidth presets - mode specific */
typedef enum {
    // CW modes (narrow)
    NOTCH_BW_CW_NARROW = 0,   // 20 Hz
    NOTCH_BW_CW_MEDIUM,       // 40 Hz  
    NOTCH_BW_CW_WIDE,         // 80 Hz
    // SSB/AM modes (wider)
    NOTCH_BW_SSB_NARROW,      // 50 Hz
    NOTCH_BW_SSB_MEDIUM,      // 100 Hz
    NOTCH_BW_SSB_WIDE,        // 200 Hz
    NOTCH_BW_COUNT
} notch_bw_preset_t;

typedef enum {
    NOTCH_DEPTH_NORMAL = 0,
    NOTCH_DEPTH_DEEP,
    NOTCH_DEPTH_COUNT
} notch_depth_t;

typedef struct {
    bool active;
    float freq_hz;           // Center frequency
    float bandwidth_hz;      // Actual computed bandwidth
    notch_depth_t depth;
    void *filter_obj;        // liquid-dsp iirnotch object
} notch_filter_t;

/* AF Leveler (Software AGC) - renamed to avoid confusion */
typedef enum {
    AF_LEVELER_OFF = 0,
    AF_LEVELER_FAST,
    AF_LEVELER_MEDIUM,
    AF_LEVELER_SLOW
} af_leveler_mode_t;

typedef struct {
    bool enabled;
    af_leveler_mode_t mode;
    float target_db;         // Target output level (-40 to -10 dB)
    float current_gain_db;   // Current gain being applied
    float peak_gain_db;      // Peak hold (1 second decay)
    float attack_rate;       // dB/sample
    float release_rate;      // dB/sample
} af_leveler_t;

/* Noise Reduction modes */
typedef enum {
    NR_OFF = 0,
    NR_BASIC,               // Fallback: gate + expander + EQ
    NR_SPEEX,               // SpeexDSP (if available)
    NR_COUNT
} nr_mode_t;

typedef struct {
    nr_mode_t mode;
    uint8_t level;          // 0-100
    void *speex_state;      // NULL if unavailable
    // Basic NR state
    float noise_gate_thresh;
    float expander_ratio;
    float hpf_state;        // High-pass filter state
    float hpf_z1;           // Previous sample
} nr_state_t;

/* Mode presets */
typedef enum {
    PRESET_OFF = 0,
    PRESET_CW_WEAK,
    PRESET_SSB_VOICE,
    PRESET_AM_BROADCAST,
    PRESET_DIGITAL,
    PRESET_COUNT
} audio_enh_preset_t;

/* Master state */
typedef struct {
    bool enabled;                       // Master switch
    audio_enh_caps_t caps;              // Runtime capabilities
    
    notch_filter_t notches[4];          // 4 notch filters
    af_leveler_t af_leveler;            // AF level control
    nr_state_t nr;                      // Noise reduction
    
    audio_enh_preset_t current_preset;
    bool preset_modified;               // User has overridden
    notch_bw_preset_t default_notch_bw; // Per-mode default
    
    // CPU monitoring
    uint8_t cpu_load;                   // 0-100%
    uint32_t last_cpu_check;
    unsigned long long prev_cpu_user;
    unsigned long long prev_cpu_system;
    unsigned long long prev_cpu_idle;
} audio_enh_state_t;

/* API Functions */

/* Lifecycle */
void audio_enh_init(void);
void audio_enh_deinit(void);
void audio_enh_process_rx(int16_t *samples, size_t nsamples);

/* Capabilities */
audio_enh_caps_t audio_enh_get_capabilities(void);
bool audio_enh_is_enabled(void);
void audio_enh_set_enabled(bool enabled);

/* Notch control */
int audio_enh_notch_add(float freq_hz, notch_bw_preset_t bw, notch_depth_t depth);
void audio_enh_notch_remove(int notch_id);
void audio_enh_notch_clear_all(void);
void audio_enh_notch_toggle_at_freq(float freq_hz);
void audio_enh_notch_set_depth(int notch_id, notch_depth_t depth);
notch_bw_preset_t audio_enh_get_default_notch_bw(void);

/* AF Leveler control */
void audio_enh_af_leveler_set_enabled(bool enabled);
void audio_enh_af_leveler_set_mode(af_leveler_mode_t mode);
void audio_enh_af_leveler_set_target(float target_db);
float audio_enh_af_leveler_get_gain_reduction(void);
float audio_enh_af_leveler_get_gain_reduction_peak(void);

/* Noise reduction */
void audio_enh_nr_set_mode(nr_mode_t mode);
void audio_enh_nr_set_level(uint8_t level);
nr_mode_t audio_enh_nr_get_effective_mode(void);  // Returns BASIC if Speex unavailable
const char *audio_enh_nr_get_mode_name(nr_mode_t mode);

/* Presets */
void audio_enh_preset_load(audio_enh_preset_t preset);
void audio_enh_preset_reset(void);
audio_enh_preset_t audio_enh_preset_get_current(void);
bool audio_enh_preset_is_modified(void);
const char *audio_enh_preset_get_name(audio_enh_preset_t preset);

/* Status */
uint8_t audio_enh_get_cpu_load(void);  // Updated at 1Hz max

#ifdef __cplusplus
}
#endif
```

#### 2.1.2 Implementation Details

**1. Capability Detection (Runtime + Build-time)**

```c
// Build-time detection via CMake
#ifdef HAVE_SPEEXDSP
    #include <speex/speex_preprocess.h>
#endif

// Runtime detection in audio_enh_init()
static void detect_capabilities(void) {
    state.caps.has_liquid_fft = true;  // Required, always available
    
    #ifdef HAVE_SPEEXDSP
        // Try to create a test Speex preprocessor
        SpeexPreprocessState *test = speex_preprocess_state_init(160, 8000);
        state.caps.has_speex = (test != NULL);
        if (test) speex_preprocess_state_destroy(test);
    #else
        state.caps.has_speex = false;
    #endif
    
    state.caps.has_pro_notch = true;  // We implement depth control
}
```

**2. Basic NR Fallback (When Speex Unavailable)**

```c
// NOT oversold as "NR equivalent" - these are "comfort tools"
static void apply_basic_nr(float *samples, size_t nsamples, nr_state_t *nr) {
    const float hpf_alpha = 0.99f;  // ~150Hz HPF at 48kHz
    const float tilt_alpha = 0.3f;  // Subtle HF boost
    
    for (size_t i = 0; i < nsamples; i++) {
        float sample = samples[i];
        
        // 1. High-pass filter (remove rumble/hum)
        nr->hpf_state = hpf_alpha * (nr->hpf_state + sample - nr->hpf_z1);
        nr->hpf_z1 = sample;
        sample = nr->hpf_state;
        
        // 2. Light noise gate (not full suppression)
        float abs_sample = fabsf(sample);
        if (abs_sample < nr->noise_gate_thresh) {
            sample *= 0.7f;  // Light attenuation
        }
        
        // 3. Tilt EQ (subtle presence boost)
        float tilt = tilt_alpha * (sample - nr->hpf_z1);
        sample += tilt * 0.1f;
        
        samples[i] = sample;
    }
}
```

**3. AF Leveler with Peak Hold**

```c
void af_leveler_process(af_leveler_t *agc, float *samples, size_t nsamples) {
    if (!agc->enabled) {
        agc->current_gain_db = 0;
        agc->peak_gain_db = 0;
        return;
    }
    
    // Pre-compute rates based on mode
    float attack_alpha = agc->attack_rate;
    float release_alpha = agc->release_rate;
    
    for (size_t i = 0; i < nsamples; i++) {
        float input_abs = fabsf(samples[i]);
        float input_db = 20.0f * log10f(input_abs + 1e-12f);
        
        // Calculate desired gain
        float desired_gain = agc->target_db - input_db;
        
        // Smooth transitions
        if (desired_gain < agc->current_gain_db) {
            // Attack (increase gain quickly)
            agc->current_gain_db += attack_alpha * (desired_gain - agc->current_gain_db);
        } else {
            // Release (decrease gain slowly)
            agc->current_gain_db += release_alpha * (desired_gain - agc->current_gain_db);
        }
        
        // Limit max gain to prevent feedback
        if (agc->current_gain_db > 30.0f) agc->current_gain_db = 30.0f;
        
        // Apply gain
        float gain_linear = powf(10.0f, agc->current_gain_db / 20.0f);
        samples[i] *= gain_linear;
    }
    
    // Peak hold with 1s decay
    static uint32_t last_peak_update = 0;
    uint32_t now = lv_tick_get();
    
    if (agc->current_gain_db < agc->peak_gain_db) {
        // New peak (more gain reduction)
        agc->peak_gain_db = agc->current_gain_db;
        last_peak_update = now;
    } else if ((now - last_peak_update) > 1000) {
        // Decay after 1 second
        agc->peak_gain_db = agc->current_gain_db;
    }
}
```

**4. Notch Filter Implementation**

```c
#include <liquid/liquid.h>

// Bandwidth mapping per preset
static const float bw_values[NOTCH_BW_COUNT] = {
    20.0f,   // CW_NARROW
    40.0f,   // CW_MEDIUM
    80.0f,   // CW_WIDE
    50.0f,   // SSB_NARROW
    100.0f,  // SSB_MEDIUM
    200.0f,  // SSB_WIDE
};

int audio_enh_notch_add(float freq_hz, notch_bw_preset_t bw_preset, notch_depth_t depth) {
    // Find first inactive slot
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        if (!state.notches[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;  // All slots full
    
    notch_filter_t *notch = &state.notches[slot];
    notch->active = true;
    notch->freq_hz = freq_hz;
    notch->bandwidth_hz = bw_values[bw_preset];
    notch->depth = depth;
    
    // Create liquid-dsp notch filter
    float q = (depth == NOTCH_DEPTH_DEEP) ? 30.0f : 15.0f;
    float fc = freq_hz / 48000.0f;  // Normalized frequency
    float fbw = notch->bandwidth_hz / 48000.0f;
    
    notch->filter_obj = iirnotch_crcf_create(fc, fbw * q);
    
    return slot;
}

void audio_enh_notch_remove(int notch_id) {
    if (notch_id < 0 || notch_id >= 4) return;
    if (!state.notches[notch_id].active) return;
    
    iirnotch_crcf_destroy((iirnotch_crcf)state.notches[notch_id].filter_obj);
    state.notches[notch_id].active = false;
}
```

**5. Lightweight CPU Monitoring**

```c
uint8_t audio_enh_get_cpu_load(void) {
    static uint32_t last_check = 0;
    static uint8_t cached_load = 0;
    
    uint32_t now = lv_tick_get();
    if ((now - last_check) < 1000) return cached_load;  // 1 Hz max
    
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return cached_load;
    
    unsigned long long user, nice, system, idle;
    fscanf(fp, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);
    fclose(fp);
    
    unsigned long long total_diff = (user - state.prev_cpu_user) + 
                                    (nice - state.prev_cpu_nice) + 
                                    (system - state.prev_cpu_system) + 
                                    (idle - state.prev_cpu_idle);
    unsigned long long busy_diff = total_diff - (idle - state.prev_cpu_idle);
    
    if (total_diff > 0) {
        cached_load = (uint8_t)((busy_diff * 100ULL) / total_diff);
    }
    
    state.prev_cpu_user = user;
    state.prev_cpu_nice = nice;
    state.prev_cpu_system = system;
    state.prev_cpu_idle = idle;
    last_check = now;
    
    return cached_load;
}
```

---

### Phase 2: Settings Dialog UI
**Duration:** 2-3 days  
**Files:** `dialog_audio_enh.h`, `dialog_audio_enh.c`

#### 2.2.1 Layout Specification

```
┌────────────────────────────────────────────┐
│ ← Audio Enhancement                [ON/OFF]│
├────────────────────────────────────────────┤
│ Preset: [SSB Voice ▼]              (mod)   │
│                                            │
│ ───── Noise Reduction ─────────────────────│
│ Mode:   [Speex ▼]                          │
│ Level:  [████░░░░░░░░░░░░░░░░░░] 40%       │
│                                            │
│ ───── AF Leveler ──────────────────────────│
│ [ ] Enabled                                │
│ Target: -25dB      Mode: [Medium ▼]        │
│ GR:    -8.5 dB  │█████░░░░░░░░░│           │
│ Peak:  -12.0 dB                            │
│                                            │
│ ───── Software Notches ────────────────────│
│ N1: [ ] 1500 Hz  100 Hz  Normal            │
│ N2: [ ] OFF                                │
│ N3: [ ] OFF                                │
│ N4: [ ] OFF                                │
│ [+ Add Notch]                              │
│                                            │
│ [Reset to Preset]    CPU: [Medium ▼]       │
└────────────────────────────────────────────┘
```

#### 2.2.2 UI Update Rates

| Element | Update Rate | Method |
|---------|-------------|--------|
| AGC Meter (instant) | 15 Hz | `lv_timer` with 66ms period |
| AGC Peak Hold | 1 Hz | Decay logic in timer callback |
| CPU Load | 1 Hz | Separate timer, reads /proc/stat |
| All other elements | On change only | Event-driven |

#### 2.2.3 "Modified" Indicator Logic

```c
// When user changes any setting:
void on_setting_changed(void) {
    if (!state.preset_modified) {
        state.preset_modified = true;
        update_preset_label();  // Show "(mod)"
    }
}

// Reset button:
void on_reset_clicked(void) {
    audio_enh_preset_load(state.current_preset);
    state.preset_modified = false;
    update_preset_label();  // Remove "(mod)"
}
```

---

### Phase 3: Audio Pipeline Integration
**Duration:** 1 day  
**Files:** `audio.c`

#### 3.1 Integration Points

```c
// In audio.c

// Add to top:
#include "audio_enhancement.h"

// Modify read_callback:
static void read_callback(pa_stream *s, size_t nbytes, void *udata) {
    int16_t *buf = NULL;
    pa_stream_peek(s, (const void**) &buf, &nbytes);
    
    // NEW: Apply audio enhancements
    audio_enh_process_rx(buf, nbytes / 2);
    
    // Existing: Pass to DSP
    dsp_put_audio_samples(nbytes / 2, buf);
    pa_stream_drop(s);
}

// Modify audio_init:
void audio_init() {
    mixer_setup();
    audio_enh_init();  // NEW
    
    // ... rest of PulseAudio setup ...
}
```

#### 3.2 Processing Function

```c
void audio_enh_process_rx(int16_t *samples, size_t nsamples) {
    if (!state.enabled) return;
    
    // Convert to float for processing
    static float float_buf[1024];  // Adjust size as needed
    if (nsamples > 1024) nsamples = 1024;
    
    for (size_t i = 0; i < nsamples; i++) {
        float_buf[i] = samples[i] / 32768.0f;  // int16 → float [-1, 1]
    }
    
    // 1. Apply notch filters
    for (int n = 0; n < 4; n++) {
        if (state.notches[n].active) {
            iirnotch_crcf notch = (iirnotch_crcf)state.notches[n].filter_obj;
            float complex y;
            for (size_t i = 0; i < nsamples; i++) {
                iirnotch_crcf_execute(notch, float_buf[i] + 0*I, &y);
                float_buf[i] = crealf(y);
            }
        }
    }
    
    // 2. Apply AF Leveler
    af_leveler_process(&state.af_leveler, float_buf, nsamples);
    
    // 3. Apply Noise Reduction
    if (state.nr.mode == NR_SPEEX && state.caps.has_speex) {
        #ifdef HAVE_SPEEXDSP
            speex_preprocess_run((SpeexPreprocessState*)state.nr.speex_state, 
                                 float_buf);  // Speex works on float buffers
        #endif
    } else if (state.nr.mode == NR_BASIC) {
        apply_basic_nr(float_buf, nsamples, &state.nr);
    }
    
    // Convert back to int16
    for (size_t i = 0; i < nsamples; i++) {
        float sample = float_buf[i] * 32768.0f;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        samples[i] = (int16_t)sample;
    }
}
```

---

### Phase 4: Hardware Shortcuts
**Duration:** 1 day  
**Files:** `hkey.c` or `buttons.cpp`

#### 4.1 Long-Press Handlers

```c
// In hkey.c or buttons.cpp

case HKEY_F3_LONG:
    // Toggle NR
    {
        nr_mode_t current = audio_enh_nr_get_effective_mode();
        nr_mode_t new_mode;
        
        if (current == NR_OFF) {
            // Try Speex first, fall back to Basic
            new_mode = audio_enh_get_capabilities().has_speex ? NR_SPEEX : NR_BASIC;
        } else {
            new_mode = NR_OFF;
        }
        
        audio_enh_nr_set_mode(new_mode);
        msg_tiny_set_text_fmt("NR: %s", audio_enh_nr_get_mode_name(new_mode));
    }
    break;

case HKEY_F4_LONG:
    // Toggle notch at current CW pitch frequency
    {
        // Get current CW pitch from radio settings
        uint16_t cw_pitch = params.cw_pitch;  // e.g., 600 Hz
        
        // Check if notch already exists at this frequency
        int existing = find_notch_near_freq(cw_pitch);
        
        if (existing >= 0) {
            audio_enh_notch_remove(existing);
            msg_tiny_set_text("Notch removed");
        } else {
            // Add with mode-appropriate BW
            notch_bw_preset_t bw = audio_enh_get_default_notch_bw();
            int id = audio_enh_notch_add(cw_pitch, bw, NOTCH_DEPTH_NORMAL);
            if (id >= 0) {
                msg_tiny_set_text_fmt("Notch %d: %d Hz", id + 1, cw_pitch);
            } else {
                msg_tiny_set_text("No notch slots free");
            }
        }
    }
    break;
```

---

### Phase 5: Visual Indicators
**Duration:** 1 day  
**Files:** `info.c`, main screen

#### 5.1 Info Panel Updates

```c
// In info.c or main_screen.c

// Add labels for:
// - SW-NR (blue when active)
// - SW-N1, SW-N2, SW-N3, SW-N4 (yellow when active)
// - AF-LVL (green when AF Leveler active)

void update_audio_enh_indicators(void) {
    // NR indicator
    if (audio_enh_nr_get_effective_mode() != NR_OFF) {
        lv_label_set_text(sw_nr_label, "SW-NR");
        lv_obj_set_style_text_color(sw_nr_label, lv_color_hex(0x4488FF), 0);  // Blue
    } else {
        lv_label_set_text(sw_nr_label, "");
    }
    
    // Notch indicators
    for (int i = 0; i < 4; i++) {
        if (state.notches[i].active) {
            lv_label_set_text_fmt(sw_notch_labels[i], "SW-N%d", i + 1);
            lv_obj_set_style_text_color(sw_notch_labels[i], 
                lv_color_hex(0xFFCC00), 0);  // Yellow/Orange
        } else {
            lv_label_set_text(sw_notch_labels[i], "");
        }
    }
    
    // AF Leveler indicator
    if (state.af_leveler.enabled) {
        lv_label_set_text(af_lvl_label, "AF-LVL");
        lv_obj_set_style_text_color(af_lvl_label, lv_color_hex(0x00CC44), 0);  // Green
    } else {
        lv_label_set_text(af_lvl_label, "");
    }
}

// Update at 5 Hz (200ms timer)
lv_timer_create(update_audio_enh_indicators, 200, NULL);
```

---

### Phase 6: Build System Integration
**Duration:** 1 day  
**Files:** `CMakeLists.txt`, buildroot configs

#### 6.1 CMakeLists.txt Changes

```cmake
# Find SpeexDSP (optional)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SPEEXDSP speexdsp)
endif()

# Audio enhancement sources
target_sources(${PROJECT_NAME} PUBLIC
    # ... existing sources ...
    audio_enhancement.c
    dialog_audio_enh.c
)

# Compile definitions
if(SPEEXDSP_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_SPEEXDSP)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SPEEXDSP_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SPEEXDSP_LIBRARIES})
    message(STATUS "✓ SpeexDSP found: ${SPEEXDSP_VERSION}")
else()
    message(STATUS "✗ SpeexDSP not found, using basic NR fallback")
endif()

# liquid-dsp is already linked
```

#### 6.2 Optional: Buildroot Package

```makefile
# In buildroot/package/speexdsp/Config.in
config BR2_PACKAGE_SPEEXDSP
    bool "speexdsp"
    help
      SpeexDSP audio processing library.
      Enables high-quality noise reduction in X6200 GUI.

# In buildroot/package/speexdsp/speexdsp.mk
SPEEXDSP_VERSION = 1.2.1
SPEEXDSP_SITE = https://gitlab.xiph.org/xiph/speexdsp/-/archive/SpeexDSP-$(SPEEXDSP_VERSION)
...
```

---

### Phase 7: Preset Configuration

#### 7.1 Preset Definitions

```c
// Preset configurations
typedef struct {
    const char *name;
    nr_mode_t nr_mode;
    uint8_t nr_level;
    bool af_leveler_enabled;
    af_leveler_mode_t af_leveler_mode;
    float af_leveler_target;
    notch_bw_preset_t default_notch_bw;
} preset_config_t;

static const preset_config_t presets[PRESET_COUNT] = {
    [PRESET_OFF] = {
        .name = "OFF",
        .nr_mode = NR_OFF,
        .nr_level = 0,
        .af_leveler_enabled = false,
        .af_leveler_mode = AF_LEVELER_MEDIUM,
        .af_leveler_target = -20.0f,
        .default_notch_bw = NOTCH_BW_SSB_MEDIUM,
    },
    
    [PRESET_CW_WEAK] = {
        .name = "CW Weak",
        .nr_mode = NR_OFF,           // Preserve tone purity
        .nr_level = 0,
        .af_leveler_enabled = true,  // Boost weak signals
        .af_leveler_mode = AF_LEVELER_MEDIUM,
        .af_leveler_target = -25.0f, // Conservative to avoid noise boost
        .default_notch_bw = NOTCH_BW_CW_MEDIUM,  // 40 Hz default
    },
    
    [PRESET_SSB_VOICE] = {
        .name = "SSB Voice",
        .nr_mode = NR_SPEEX,         // Use best available
        .nr_level = 40,              // Moderate NR
        .af_leveler_enabled = false, // Trust hardware AGC
        .af_leveler_mode = AF_LEVELER_MEDIUM,
        .af_leveler_target = -20.0f,
        .default_notch_bw = NOTCH_BW_SSB_MEDIUM,  // 100 Hz
    },
    
    [PRESET_AM_BROADCAST] = {
        .name = "AM Broadcast",
        .nr_mode = NR_SPEEX,
        .nr_level = 60,              // Aggressive NR for noisy AM
        .af_leveler_enabled = true,
        .af_leveler_mode = AF_LEVELER_SLOW,
        .af_leveler_target = -18.0f,
        .default_notch_bw = NOTCH_BW_SSB_WIDE,  // 200 Hz for AM carriers
    },
    
    [PRESET_DIGITAL] = {
        .name = "Digital",
        .nr_mode = NR_OFF,           // Clean audio for decoding
        .nr_level = 0,
        .af_leveler_enabled = false,
        .af_leveler_mode = AF_LEVELER_MEDIUM,
        .af_leveler_target = -20.0f,
        .default_notch_bw = NOTCH_BW_SSB_NARROW,
    },
};

void audio_enh_preset_load(audio_enh_preset_t preset) {
    const preset_config_t *cfg = &presets[preset];
    
    // Load effective NR mode (handle Speex unavailable)
    nr_mode_t effective_nr = cfg->nr_mode;
    if (effective_nr == NR_SPEEX && !state.caps.has_speex) {
        effective_nr = NR_BASIC;
    }
    
    audio_enh_nr_set_mode(effective_nr);
    audio_enh_nr_set_level(cfg->nr_level);
    audio_enh_af_leveler_set_enabled(cfg->af_leveler_enabled);
    audio_enh_af_leveler_set_mode(cfg->af_leveler_mode);
    audio_enh_af_leveler_set_target(cfg->af_leveler_target);
    
    state.current_preset = preset;
    state.preset_modified = false;
    state.default_notch_bw = cfg->default_notch_bw;
}
```

---

## 3. Testing Plan

### 3.1 Unit Tests

| Test | Method | Expected Result |
|------|--------|----------------|
| Notch filter | Inject sine wave at notch freq | >40dB attenuation |
| AF Leveler | Ramp input from -40dB to -10dB | Output stabilizes at target |
| Basic NR | White noise input | Rumble removed, mild attenuation |
| Speex NR | Voice in noise | Noise reduced, voice preserved |
| CPU monitoring | Heavy processing load | Accurate % reported |

### 3.2 Integration Tests

| Scenario | Steps | Verification |
|----------|-------|--------------|
| CW Weak signal | Enable preset, listen to weak CW | Tone clear, noise manageable |
| SSB QSO | Enable preset, conduct QSO | Natural voice, no artifacts |
| Multiple notches | Add 3 notches at birdies | All carriers suppressed |
| Battery mode | Run with all processing | <20% CPU, stable audio |
| Fallback mode | Build without Speex | Basic NR functions correctly |

### 3.3 Field Tests

- [ ] SOTA activation (4+ hours battery)
- [ ] Weak signal DX (CW)
- [ ] SSB net participation
- [ ] QRM-heavy environment (notch test)
- [ ] Cold weather operation (<0°C)

---

## 4. Performance Targets

| Metric | Target | Maximum |
|--------|--------|---------|
| Latency (notches only) | <1ms | <2ms |
| Latency (full chain) | <25ms | <50ms |
| CPU (notches only) | 3% per notch | 5% per notch |
| CPU (full chain) | 15% | 25% |
| Memory (code) | 15KB | 20KB |
| Memory (RAM) | 8KB | 16KB |
| Battery impact | +5-8% | +15% |

---

## 5. File Summary

### New Files
| File | Lines | Purpose |
|------|-------|---------|
| `audio_enhancement.h` | ~150 | Public API |
| `audio_enhancement.c` | ~800 | Core implementation |
| `dialog_audio_enh.h` | ~30 | Settings dialog header |
| `dialog_audio_enh.c` | ~600 | Settings dialog UI |

### Modified Files
| File | Changes |
|------|---------|
| `audio.c` | Add `audio_enh_process_rx()` call in callback |
| `audio.h` | Add enhancement enable/disable |
| `params/params.h` | Add parameters for NR, AGC, notches, preset |
| `params/params.c` | Load/save enhancement parameters |
| `buttons.cpp` | Add long-press handlers for F3/F4 |
| `main_screen.c` | Add info panel indicators |
| `CMakeLists.txt` | Add sources, detect SpeexDSP |

---

## 6. Future Enhancements (Phase 2)

1. **RNNoise Integration** - Neural NR (higher quality, higher CPU)
2. **Audio Scope** - Real-time FFT showing filter action
3. **Auto-Notch Enhancement** - Auto-detect and track carriers
4. **CW Matched Filter** - Optimal keying envelope detection
5. **Multi-band Profiles** - Different settings per band/memory
6. **Recording** - Save processed audio to SD card

---

## 7. Notes

### Battery Life Considerations
- Always allow user to disable all processing (PRESET_OFF)
- CPU monitoring helps users understand power cost
- Consider auto-disabling when battery <20%

### QRP Philosophy
- "Every milliwatt counts" - processing is optional
- "Every millisecond of latency matters" - <25ms target
- "Every tap should feel responsive" - UI updates at 15Hz max

### Comparison to Professional Rigs
| Feature | X6200 HW | X6200+Software | IC-7300 | FTDX10 |
|---------|----------|----------------|---------|--------|
| Notches | 1 | **4+1** | 1-2 | 8 |
| NR | Basic | **Good/Basic** | Good | Excellent |
| AGC Display | Text | **Gain reduction meter** | Audio scope | Good |
| CPU cost | 0% | 10-15% | N/A | N/A |

**Result:** Matches or exceeds IC-7300 filtering capability!

---

**Ready for implementation. Estimated total time: 8-10 days.**

*Remember: This enhancement transforms X6200 from "good budget radio" to "professional-grade filtering" while maintaining QRP power efficiency.*

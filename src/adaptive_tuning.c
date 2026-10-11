/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Adaptive Tuning Implementation
 *
 *  Copyright (c) 2024 LU2DSX
 */

#include "adaptive_tuning.h"
#include "lvgl/lvgl.h"
#include <math.h>

adaptive_tuning_state_t g_adaptive_tuning = {0};

void adaptive_tuning_init(adaptive_tuning_state_t *state) {
    if (!state) return;
    
    memset(state, 0, sizeof(adaptive_tuning_state_t));
    state->current_multiplier = 1.0f;
    state->smoothed_speed = 0.0f;
    state->active = true;
    state->last_update_ms = lv_tick_get();
    state->last_event_ms = lv_tick_get();
}

void adaptive_tuning_add_event(adaptive_tuning_state_t *state, int32_t diff, uint32_t timestamp_ms) {
    if (!state || !state->active) return;
    
    // Add to circular buffer
    uint8_t idx = state->history_index;
    state->history[idx].timestamp_ms = timestamp_ms;
    state->history[idx].diff = diff;
    
    state->history_index = (idx + 1) % ADAPTIVE_TUNING_HISTORY_SIZE;
    
    if (state->event_count < ADAPTIVE_TUNING_HISTORY_SIZE) {
        state->event_count++;
    }
    
    state->last_event_ms = timestamp_ms;
}

float adaptive_tuning_calc_speed(adaptive_tuning_state_t *state) {
    if (!state || state->event_count < 2) return 0.0f;
    
    // Find oldest and newest events
    uint8_t oldest_idx = (state->history_index + ADAPTIVE_TUNING_HISTORY_SIZE - state->event_count) 
                         % ADAPTIVE_TUNING_HISTORY_SIZE;
    uint8_t newest_idx = (state->history_index + ADAPTIVE_TUNING_HISTORY_SIZE - 1) 
                         % ADAPTIVE_TUNING_HISTORY_SIZE;
    
    uint32_t time_span = state->history[newest_idx].timestamp_ms - state->history[oldest_idx].timestamp_ms;
    
    if (time_span < 50) return 0.0f;  // Minimum 50ms for valid measurement
    
    int32_t total_clicks = 0;
    for (uint8_t i = 0; i < state->event_count; i++) {
        uint8_t idx = (oldest_idx + i) % ADAPTIVE_TUNING_HISTORY_SIZE;
        total_clicks += abs(state->history[idx].diff);
    }
    
    return (float)total_clicks / (time_span / 1000.0f);  // clicks per second
}

float smooth_speed(float current, float new_reading) {
    if (current <= 0.0f) return new_reading;  // First reading
    return (SPEED_SMOOTHING_ALPHA * new_reading) + ((1.0f - SPEED_SMOOTHING_ALPHA) * current);
}

float speed_to_multiplier(float speed) {
    if (speed <= 2.0f) {
        // Ultra-fine tuning: 0.1× (0.1 step increments)
        return 0.1f;
    } else if (speed <= 4.0f) {
        // Fine tuning: 0.1× to 1.0×
        return 0.1f + (speed - 2.0f) * 0.45f;
    } else if (speed <= 6.0f) {
        // Normal: 1.0× to 2.0×
        return 1.0f + (speed - 4.0f) * 0.5f;
    } else if (speed <= 10.0f) {
        // Fast: 2.0× to 5.0×
        return 2.0f + (speed - 6.0f) * 0.75f;
    } else if (speed <= 20.0f) {
        // Very fast: 5.0× to 20.0×
        return 5.0f + (speed - 10.0f) * 1.5f;
    } else {
        // Band scanning: 20.0× to 100.0×, capped
        float mult = 20.0f + (speed - 20.0f) * 4.0f;
        return (mult > 100.0f) ? 100.0f : mult;
    }
}

float adaptive_multiplier_gate(float multiplier, adaptive_accel_t accel) {
    if (multiplier < 0.1f) {
        multiplier = 0.1f;
    }

    switch (accel) {
        case ADAPTIVE_ACCEL_NONE:
            return 1.0f;

        case ADAPTIVE_ACCEL_LITE:
            return (multiplier > 10.0f) ? 10.0f : multiplier;

        case ADAPTIVE_ACCEL_STRONG:
        default:
            return multiplier;
    }
}

void adaptive_tuning_update(adaptive_tuning_state_t *state) {
    if (!state || !state->active) return;
    
    uint32_t now = lv_tick_get();
    state->last_update_ms = now;
    
    // Check for timeout (no events for 500ms)
    if ((now - state->last_event_ms) > EVENT_TIMEOUT_MS) {
        // Reset to normal speed
        state->smoothed_speed = 0.0f;
        state->current_multiplier = 1.0f;
        state->event_count = 0;
        return;
    }
    
    // Calculate speed
    float instant_speed = adaptive_tuning_calc_speed(state);
    
    // Apply smoothing
    state->smoothed_speed = smooth_speed(state->smoothed_speed, instant_speed);
    
    // Calculate multiplier
    state->current_multiplier = speed_to_multiplier(state->smoothed_speed);
}

void adaptive_tuning_decay(adaptive_tuning_state_t *state) {
    if (!state) return;
    
    uint32_t now = lv_tick_get();
    
    // Only decay if no recent events
    if ((now - state->last_event_ms) <= EVENT_TIMEOUT_MS) return;
    
    // Decay toward 1.0
    if (state->current_multiplier > 1.0f) {
        state->current_multiplier *= MULTIPLIER_DECAY_RATE;
        if (state->current_multiplier < 1.0f) {
            state->current_multiplier = 1.0f;
        }
    } else if (state->current_multiplier < 1.0f) {
        state->current_multiplier /= MULTIPLIER_DECAY_RATE;
        if (state->current_multiplier > 1.0f) {
            state->current_multiplier = 1.0f;
        }
    }
    
    // Reset speed when fully decayed
    if (fabsf(state->current_multiplier - 1.0f) < 0.01f) {
        state->smoothed_speed = 0.0f;
        state->event_count = 0;
    }
}

uint32_t multiplier_to_color(float multiplier) {
    if (multiplier < 1.0f) {
        // Blue for fine tuning (×0.1 to ×0.9)
        uint8_t intensity = (uint8_t)(255 * (multiplier - 0.1f) / 0.9f);
        return 0x0000FF | (intensity << 16) | (intensity << 8);  // Blue to light blue
    } else if (multiplier < 2.0f) {
        // White for normal (×1.0 to ×1.9)
        return 0xFFFFFF;
    } else if (multiplier < 10.0f) {
        // Yellow for fast (×2.0 to ×9.9)
        uint8_t intensity = (uint8_t)(128 + 127 * (multiplier - 2.0f) / 8.0f);
        return 0xFFCC00 | (intensity << 16);  // Yellow-orange
    } else {
        // Red for scan mode (×10+)
        uint8_t intensity = (uint8_t)(255 * ((multiplier > 50.0f) ? 1.0f : multiplier / 50.0f));
        return 0xFF0000 | ((255 - intensity) << 8);  // Red to bright red
    }
}

const char *multiplier_color_name(float multiplier) {
    if (multiplier < 1.0f) return "fine";
    if (multiplier < 2.0f) return "normal";
    if (multiplier < 10.0f) return "fast";
    return "scan";
}

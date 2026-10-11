/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Adaptive Tuning
 *
 *  Copyright (c) 2024 LU2DSX
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADAPTIVE_TUNING_HISTORY_SIZE 8
#define SPEED_SMOOTHING_ALPHA 0.4f
#define MULTIPLIER_DECAY_RATE 0.9f
#define MULTIPLIER_DECAY_INTERVAL_MS 100
#define EVENT_TIMEOUT_MS 500

typedef struct {
    uint32_t timestamp_ms;
    int32_t diff;
} tuning_event_t;

typedef struct {
    tuning_event_t history[ADAPTIVE_TUNING_HISTORY_SIZE];
    uint8_t event_count;
    uint8_t history_index;
    float smoothed_speed;           // clicks per second
    float current_multiplier;       // current step multiplier
    uint32_t last_update_ms;
    uint32_t last_event_ms;
    bool active;
} adaptive_tuning_state_t;

/* Event structure for enhanced rotary events */
typedef struct {
    int32_t raw_diff;               // Raw encoder diff
    float multiplier;               // Calculated multiplier
    float effective_step;           // Base step × multiplier
} rotary_event_enhanced_t;

/* Acceleration tiers, must match freq_accel_t in params/params.h
 * (a _Static_assert in main_screen.c enforces it at compile time). */
typedef enum {
    ADAPTIVE_ACCEL_NONE = 0,
    ADAPTIVE_ACCEL_LITE,
    ADAPTIVE_ACCEL_STRONG,
} adaptive_accel_t;

/* Gate the adaptive multiplier with the user "Freq acceleration" setting:
 * none = 1x (no acceleration), lite = up to 10x, strong = up to 100x. */
float adaptive_multiplier_gate(float multiplier, adaptive_accel_t accel);

/* Initialize adaptive tuning system */
void adaptive_tuning_init(adaptive_tuning_state_t *state);

/* Add a new rotary event */
void adaptive_tuning_add_event(adaptive_tuning_state_t *state, int32_t diff, uint32_t timestamp_ms);

/* Update speed and multiplier calculations */
void adaptive_tuning_update(adaptive_tuning_state_t *state);

/* Calculate instantaneous speed from event history */
float adaptive_tuning_calc_speed(adaptive_tuning_state_t *state);

/* Convert speed to step multiplier */
float speed_to_multiplier(float speed);

/* Apply exponential smoothing to speed */
float smooth_speed(float current, float new_reading);

/* Decay multiplier back to 1.0 when idle */
void adaptive_tuning_decay(adaptive_tuning_state_t *state);

/* Get color for multiplier display */
uint32_t multiplier_to_color(float multiplier);

/* Get color name for multiplier */
const char *multiplier_color_name(float multiplier);

/* Global state instance */
extern adaptive_tuning_state_t g_adaptive_tuning;

#ifdef __cplusplus
}
#endif

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Voice Stub
 *
 *  Stub header to replace voice.h for power-efficient builds without TTS.
 *  All voice functions are no-ops to minimize power consumption.
 */

#pragma once

#ifdef __cplusplus

#include <cstdarg>

extern "C" {
#include <stdint.h>
#endif

#define VOICES_NUM 1

typedef enum {
    VOICE_OFF = 0,
    VOICE_LCD,
    VOICE_ALWAYS
} voice_mode_t;

/* Stub functions - no actual voice synthesis to save power */
static inline void voice_sure() { }
static inline void voice_change_mode() { }

static inline void voice_say_text_fmt(const char * fmt, ...) {
    (void)fmt;
}

static inline void voice_delay_say_text_fmt(const char * fmt, ...) {
    (void)fmt;
}

static inline void voice_say_freq(uint64_t freq) {
    (void)freq;
}

static inline void voice_say_bool(const char *prompt, bool x) {
    (void)prompt;
    (void)x;
}

static inline void voice_say_int(const char *prompt, int32_t x) {
    (void)prompt;
    (void)x;
}

static inline void voice_say_float(const char *prompt, float x) {
    (void)prompt;
    (void)x;
}

static inline void voice_say_float2(const char *prompt, float x) {
    (void)prompt;
    (void)x;
}

static inline void voice_say_text(const char *prompt, const char *x) {
    (void)prompt;
    (void)x;
}

static inline void voice_say_lang() { }

static inline const char * voice_change(int16_t diff) {
    (void)diff;
    return "Off";
}

#ifdef __cplusplus
}
#endif

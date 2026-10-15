/*
 * Stub implementations for the host/ARM unit tests (see docs/TESTING.md).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>
#include <stdlib.h>

#include "lvgl/lvgl.h"
#include "liquid/liquid.h"
#include "speex/speex_preprocess.h"

/* La firma del nivel del backlight que este stub implementa (misma declaracion
 * que ve el ROM y el test: src/backlight_level.h). */
#include "backlight_level.h"

/* ---------------- LVGL ---------------- */

static uint32_t test_tick = 0;

uint32_t lv_tick_get(void) {
    return test_tick;
}

void test_tick_set(uint32_t ms) {
    test_tick = ms;
}

void test_tick_advance(uint32_t ms) {
    test_tick += ms;
}

/* ---------------- liquid-dsp (pass-through notch) ---------------- */

struct firfilt_rrrf_s {
    float last;
};

firfilt_rrrf firfilt_rrrf_create_notch(unsigned int m, float As, float f0) {
    (void) m;
    (void) As;
    (void) f0;

    return calloc(1, sizeof(struct firfilt_rrrf_s));
}

void firfilt_rrrf_destroy(firfilt_rrrf q) {
    free(q);
}

void firfilt_rrrf_push(firfilt_rrrf q, float x) {
    q->last = x;
}

void firfilt_rrrf_execute(firfilt_rrrf q, float *y) {
    *y = q->last;
}

/* ---------------- SpeexDSP preprocessor ---------------- */

struct SpeexPreprocessState_ {
    int frame_size;
    int denoise;
    int noise_suppress;
};

static int speex_frames_processed = 0;

SpeexPreprocessState *speex_preprocess_state_init(int frame_size, int sampling_rate) {
    (void) sampling_rate;

    SpeexPreprocessState *st = calloc(1, sizeof(*st));

    if (st) {
        st->frame_size = frame_size;
    }

    return st;
}

void speex_preprocess_state_destroy(SpeexPreprocessState *st) {
    free(st);
}

int speex_preprocess_ctl(SpeexPreprocessState *st, int request, void *ptr) {
    if (!st || !ptr) {
        return -1;
    }

    if (request == SPEEX_PREPROCESS_SET_DENOISE) {
        st->denoise = *(int *) ptr;
    } else if (request == SPEEX_PREPROCESS_SET_NOISE_SUPPRESS) {
        st->noise_suppress = *(int *) ptr;
    }

    return 0;
}

int speex_preprocess_run(SpeexPreprocessState *st, int16_t *x) {
    if (!st) {
        return 0;
    }

    /* Deterministic: attenuate 6 dB so tests can detect that it ran. */
    for (int i = 0; i < st->frame_size; i++) {
        x[i] = (int16_t) (x[i] / 2);
    }

    speex_frames_processed++;

    return 1;
}

int test_speex_frames(void) {
    return speex_frames_processed;
}

/* ---------------- waterfall: wiring de la politica visual ---------------- */

/* src/visual_policy.c aplica el perfil al modo Ahorro del waterfall
 * (UI-08 incr. 2). El test nativo no linkea waterfall.c, asi que este stub
 * registra la ultima llamada (y cuantas hubo) para poder asertar la decision
 * de la politica sin el render ni el flujo de radio. */
static bool waterfall_battery_mode;
static int  waterfall_battery_calls;

void waterfall_set_battery_mode(bool enabled) {
    waterfall_battery_mode = enabled;
    waterfall_battery_calls++;
}

bool test_waterfall_battery_mode(void) {
    return waterfall_battery_mode;
}

int test_waterfall_battery_calls(void) {
    return waterfall_battery_calls;
}

void test_waterfall_battery_reset(void) {
    waterfall_battery_mode = false;
    waterfall_battery_calls = 0;
}

/* ---------------- spectrum: wiring de la cadencia ECO ---------------- */

/* src/visual_policy.c aplica el perfil a la cadencia del espectro
 * (visual_policy_spectrum_apply -> spectrum_set_refresh_period). Como el
 * waterfall, el test no linkea spectrum.c: este stub registra el ultimo periodo
 * (y cuantas veces se lo fijo) para asertar la decision de la politica. */
static uint8_t spectrum_period;
static int     spectrum_period_calls;

void spectrum_set_refresh_period(uint8_t frames) {
    spectrum_period = frames;
    spectrum_period_calls++;
}

uint8_t test_spectrum_period(void) {
    return spectrum_period;
}

int test_spectrum_period_calls(void) {
    return spectrum_period_calls;
}

void test_spectrum_period_reset(void) {
    spectrum_period = 0;
    spectrum_period_calls = 0;
}

/* ---------------- backlight: wiring del brillo de la politica visual -------- */

/* src/visual_policy.c aplica el perfil al nivel del backlight (UI-08 incr. 3)
 * por backlight_set_brightness() (src/backlight_level.h). El test nativo no
 * linkea backlight.c (sysfs y GPIO reales), asi que este stub registra la
 * ultima llamada (y cuantas hubo): la decision de la politica se comprueba sin
 * la senal de hardware, que es de la pasada fisica. */
static int16_t backlight_brightness;
static int     backlight_brightness_calls;

void backlight_set_brightness(int16_t value) {
    backlight_brightness = value;
    backlight_brightness_calls++;
}

int16_t test_backlight_brightness_level(void) {
    return backlight_brightness;
}

int test_backlight_brightness_calls(void) {
    return backlight_brightness_calls;
}

void test_backlight_brightness_reset(void) {
    backlight_brightness = 0;
    backlight_brightness_calls = 0;
}

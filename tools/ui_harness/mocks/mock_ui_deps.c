/*
 *  Host stubs for everything the rendered GUI components call that is not a
 *  rendered component itself: the event bus, the message bars, voice, the
 *  Bluetooth/WiFi/backlight modules and the monotonic clock.
 *
 *  Every stub has a documented, fixed answer. In particular get_time() reads
 *  the harness' virtual clock (virtual_time.h), never the wall clock, so the
 *  rendered state can never depend on how long the process has run.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl/lvgl.h"

#include "backlight.h"
#include "btaudio.h"
#include "bt_state.h"
#include "events.h"
#include "msg.h"
#include "msg_tiny.h"
#include "spectrum.h"
#include "util.h"
#include "voice.h"
#include "wifi.h"

#include "virtual_time.h"

/* --- event bus -------------------------------------------------------------
 * The custom event ids must be allocated by LVGL (lv_event_register_id), exactly
 * as the firmware's events.c does: LVGL stores the callback filter in an 8-bit
 * field, so an id above 255 would silently become LV_EVENT_ALL and dispatch
 * every event to every callback. event_init() must run before any screen is
 * built. event_send() is a no-op here: the harness pushes state directly. */

uint32_t EVENT_ROTARY_ENHANCED;
uint32_t EVENT_KEYPAD;
uint32_t EVENT_HKEY;
uint32_t EVENT_RADIO_TX;
uint32_t EVENT_RADIO_RX;
uint32_t EVENT_SCREEN_UPDATE;
uint32_t EVENT_MSG_UPDATE;
uint32_t EVENT_BAND_UP;
uint32_t EVENT_BAND_DOWN;

void event_init(void)
{
    EVENT_ROTARY_ENHANCED = lv_event_register_id();
    EVENT_KEYPAD          = lv_event_register_id();
    EVENT_HKEY            = lv_event_register_id();
    EVENT_RADIO_TX        = lv_event_register_id();
    EVENT_RADIO_RX        = lv_event_register_id();
    EVENT_SCREEN_UPDATE   = lv_event_register_id();
    EVENT_MSG_UPDATE      = lv_event_register_id();
    EVENT_BAND_UP         = lv_event_register_id();
    EVENT_BAND_DOWN       = lv_event_register_id();
}

void event_obj_check(void) {}

void event_send(lv_obj_t *obj, lv_event_code_t event_code, void *param)
{
    (void) obj;
    (void) event_code;
    (void) param;
}

void event_send_key(int32_t key)
{
    (void) key;
}

/* --- virtual clock and integer helpers ------------------------------------- */

uint64_t get_time(void)
{
    return virtual_time_now_ms();
}

void get_time_str(char *str, size_t str_size)
{
    snprintf(str, str_size, "%02u:%02u:%02u", 0u, 0u, 0u);
}

void boot_mark(const char *etapa)
{
    (void) etapa;
}

void split_freq(int32_t freq, uint16_t *mhz, uint16_t *khz, uint16_t *hz)
{
    *mhz = freq / 1000000;
    *khz = (freq / 1000) % 1000;
    *hz  = freq % 1000;
}

int32_t align_int(int32_t x, uint16_t step)
{
    if (step == 0) {
        return x;
    }
    return x - (x % step);
}

uint64_t align_long(uint64_t x, uint16_t step)
{
    if (step == 0) {
        return x;
    }
    return x - (x % step);
}

int32_t limit(int32_t x, int32_t min, int32_t max)
{
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

int sign(int x)
{
    return x > 0 ? 1 : x < 0 ? -1 : 0;
}

/* util.cpp's low-pass filter and mode stepper, re-implemented here so the
 * harness does not pull util.cpp's get_time() (which reads the wall clock) and
 * keeps every value on the virtual clock. Same behaviour as src/util.cpp. */

void lpf(float *x, float current, float beta, float initial)
{
    if (*x == initial) {
        *x = current;
    } else {
        *x = *x * beta + current * (1.0f - beta);
    }
}

int loop_modes(int16_t dir, int mode, uint64_t modes, const int max_val)
{
    /* Same stepping as src/util.cpp: advance one slot in `dir` and stop on the
     * first enabled mode. The firmware loops forever when dir == 0 and the
     * current mode is disabled; the harness caps the search so it can never
     * hang a render. */
    int guard = 0;
    int cap = 2 * (max_val + 1) + 1;

    while (1) {
        if (dir > 0) {
            mode = (mode == max_val) ? 0 : mode + 1;
        } else if (dir < 0) {
            mode = (mode == 0) ? max_val : mode - 1;
        }

        if (modes & ((uint64_t) 1 << mode)) {
            break;
        }
        if (++guard > cap) {
            break;
        }
    }

    return mode;
}

/* --- voice ----------------------------------------------------------------- */

void voice_sure(void) {}
void voice_ready(void) {}
bool voice_selftest(void) { return false; }
void voice_change_mode(void) {}
void voice_say_text_fmt(const char *fmt, ...) { (void) fmt; }
void voice_delay_say_text_fmt(const char *fmt, ...) { (void) fmt; }
void voice_say_freq(uint64_t freq) { (void) freq; }
void voice_say_bool(const char *p, bool x) { (void) p; (void) x; }
void voice_say_int(const char *p, int32_t x) { (void) p; (void) x; }
void voice_say_float(const char *p, float x) { (void) p; (void) x; }
void voice_say_float2(const char *p, float x) { (void) p; (void) x; }
void voice_say_text(const char *p, const char *x) { (void) p; (void) x; }
void voice_say_lang(void) {}
const char *voice_change(int16_t diff) { (void) diff; return NULL; }

/* --- Bluetooth (off), WiFi (off), backlight -------------------------------- */

bool bt_state_on(void) { return false; }
bool bt_state_discoverable(void) { return false; }
bool bt_state_connected(void) { return false; }
bool bt_state_mute(void) { return false; }

void btaudio_log(const char *fmt, ...) { (void) fmt; }
void btaudio_run(const char *fmt, ...) { (void) fmt; }
void btaudio_headphone_volume(int pct) { (void) pct; }
void btaudio_speaker_mute(bool on) { (void) on; }
void btaudio_follow_state(void) {}

wifi_status_t wifi_get_status(void)
{
    return WIFI_OFF;
}

void backlight_init(void) {}
void backlight_tick(void) {}
void backlight_set_brightness(int16_t value) { (void) value; }
void backlight_set_buttons(buttons_light_t value) { (void) value; }
void backlight_switch(void) {}
bool backlight_is_on(void) { return true; }

/*
 *  Host stubs for everything the rendered GUI components call that is not a
 *  rendered component itself: the event bus, the message bars, voice, the
 *  Bluetooth/WiFi/backlight modules, the audio volume surface, the wfview
 *  server, the audio-enhancement presets and the clocks.
 *
 *  Every stub has a documented, fixed answer. In particular get_time() reads
 *  the harness' virtual clock (virtual_time.h) and __wrap_time() returns a
 *  fixed epoch, never the wall clock, so the rendered state can never depend on
 *  how long the process has run or on what the day is when it runs.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lvgl/lvgl.h"

#include "audio.h"
#include "audio_enhancement.h"
#include "backlight.h"
#include "btaudio.h"
#include "bt_state.h"
#include "events.h"
#include "msg.h"
#include "msg_tiny.h"
#include "spectrum.h"
#include "util.h"
#include "voice.h"
#include "wfview_server.h"
#include "wifi.h"

#include "mock_harness.h"
#include "virtual_time.h"

/* --- wall clock ------------------------------------------------------------
 * `src/dialog_settings.c` fills its Day/Month/Year and Hour/Min/Sec fields from
 * `time()` + `localtime()` (the RTC the radio sets). The harness has no RTC and
 * must not depend on the host clock —a fixture that renders the current date
 * would change every second—, so the link wraps every `time()` call of the
 * firmware sources onto this fixed epoch (CMakeLists.txt: `--wrap=time`). The
 * TZ is pinned to UTC by harness_main.c so `localtime()` is fixed too. The
 * value is arbitrary but constant: 2026-01-01 00:00:00 UTC. */

#define HARNESS_FIXED_EPOCH ((time_t) 1767225600)

time_t __wrap_time(time_t *tloc)
{
    if (tloc != NULL) {
        *tloc = HARNESS_FIXED_EPOCH;
    }
    return HARNESS_FIXED_EPOCH;
}

/* --- audio volume ----------------------------------------------------------
 * audio_set_play_vol()/audio_set_rec_vol() (src/audio.c) drive the ALSA mixer
 * and return the level the codec accepted; when the device is absent they
 * return the requested value (src/audio.c:387/428). The harness never
 * reaches a device, so the mixer is "absent": the requested value comes back,
 * which is exactly what the firmware does on a machine without that mixer.
 *
 * They are also a recorder, the same idiom as the backlight test's policy stub:
 * both sliders of Ajustes -> Audio look identical and their callbacks differ only
 * in which of these two they call, so which one ran is the only observation point
 * there is. The `settings-audio` fixture fires the Rec slider and checks the
 * recorded setter: with the two wired to the same function (issue #26) the
 * recorder stays on `play` and the fixture stops the run. */
static mock_audio_setter_t audio_last = MOCK_AUDIO_NONE;
static float               audio_last_db;
static int                 audio_calls[3];

mock_audio_setter_t mock_audio_last_setter(void) { return audio_last; }
float               mock_audio_last_db(void) { return audio_last_db; }

int mock_audio_calls(mock_audio_setter_t which) {
    return (which >= MOCK_AUDIO_NONE && which <= MOCK_AUDIO_REC) ? audio_calls[which] : 0;
}

void mock_audio_reset(void) {
    audio_last    = MOCK_AUDIO_NONE;
    audio_last_db = 0.0f;
    audio_calls[MOCK_AUDIO_NONE] = 0;
    audio_calls[MOCK_AUDIO_PLAY] = 0;
    audio_calls[MOCK_AUDIO_REC]  = 0;
}

static float audio_record(mock_audio_setter_t which, float db) {
    audio_last    = which;
    audio_last_db = db;
    audio_calls[which]++;
    return db;
}

float audio_set_play_vol(float db) { return audio_record(MOCK_AUDIO_PLAY, db); }
float audio_set_rec_vol(float db) { return audio_record(MOCK_AUDIO_REC, db); }

/* --- audio enhancement presets ---------------------------------------------
 * The Ajustes "Audio enhance" dropdown loads a DSP preset (src/
 * audio_enhancement.c, speex + liquid). No FFT/DSP runs on the host, so the
 * preset load is a no-op; the parameter the dropdown writes (params.
 * audio_enh_preset) is the mock store's. */
void audio_enh_preset_load(audio_enh_preset_t preset) { (void) preset; }

/* --- wfview (Icom LAN) server ---------------------------------------------
 * The real wfview_server reads /mnt/wfview.conf and opens UDP ports (pulse,
 * cat). The harness owns no file and no socket: the server is off, so the
 * Ajustes switch draws unchecked and enabling it is a no-op. */
bool wfview_server_get_enabled(void) { return false; }
void wfview_server_set_enabled(bool on) { (void) on; }

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

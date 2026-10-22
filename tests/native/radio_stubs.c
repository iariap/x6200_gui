/*
 * Shared stubs and hooks for the radio_thread tests (issue #219).
 * See tests/native/radio_stubs.h for the observable surface.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "radio_stubs.h"

#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lvgl/lvgl.h"

#include "aether_radio/x6200_control/control.h"
#include "aether_radio/x6200_control/low/flow.h"

#include "cfg/subjects.h"
#include "cfg/atu.h"
#include "cfg/cfg.h"
#include "cfg/transverter.h"
#include "params/params.h"
#include "btaudio.h"
#include "cb.h"
#include "clock.h"
#include "dsp.h"
#include "dialog_swrscan.h"
#include "events.h"
#include "gpio_safe.h"
#include "hkey.h"
#include "main.h"
#include "msg.h"
#include "scheduler.h"
#include "sql_policy.h"
#include "tx_info.h"
#include "util.h"
#include "visual_policy.h"
#include "voice.h"

radio_stub_clock_t radio_stub_clock = RADIO_STUB_CLOCK_FAKE;
uint64_t           radio_stub_fake_ms;

/* Data symbols src/radio.c reads. Zero-initialized: the reachable paths only
 * pass the Subject pointers through to stubs and read the visual-policy params
 * (waterfall_battery/brightness_normal/visual_profile), so the zero value is
 * inert. cfg_cur.band stays NULL and is never dereferenced by radio_thread or
 * radio_tick (the band items are only touched by the observer callbacks). */
params_t   params;
cfg_t      cfg;
cfg_cur_t  cfg_cur;
uint32_t   EVENT_KEYPAD;

int      radio_stub_sleep_calls;
uint64_t radio_stub_sleep_us_total;
bool     radio_stub_saw_bad_sleep;
int      radio_stub_sleep_limit;
jmp_buf  radio_stub_jmp;
bool     radio_stub_jmp_armed;
volatile bool radio_stub_stop;
bool     radio_stub_exit_on_stop;

bool     radio_stub_flow_ok;
const bool *radio_stub_flow_pattern;
int      radio_stub_flow_pattern_len;
int      radio_stub_flow_read_limit;
uint64_t radio_stub_flow_reads;
int      radio_stub_idle_calls;
int      radio_stub_poll_calls;
int      radio_stub_swrscan_updates;
int      radio_stub_dsp_calls;
int      radio_stub_idle_marks[RADIO_STUB_IDLE_MARKS];
int      radio_stub_idle_marks_n;

/* Audio de recepcion (issue #244), ver radio_stubs.h. */
int32_t  radio_stub_vol;
int32_t  radio_stub_mode;
int32_t  radio_stub_subject_set_last;
int      radio_stub_subject_set_calls;
int      radio_stub_rxvol_calls;
uint8_t  radio_stub_rxvol_last;
bool     radio_stub_bt_muted;

void radio_stub_reset(void) {
    radio_stub_clock          = RADIO_STUB_CLOCK_FAKE;
    radio_stub_fake_ms        = 0;
    radio_stub_sleep_calls    = 0;
    radio_stub_sleep_us_total = 0;
    radio_stub_saw_bad_sleep  = false;
    radio_stub_sleep_limit    = 0;
    radio_stub_jmp_armed      = false;
    radio_stub_stop           = false;
    radio_stub_exit_on_stop   = false;
    radio_stub_flow_ok        = false;
    radio_stub_flow_pattern   = NULL;
    radio_stub_flow_pattern_len = 0;
    radio_stub_flow_read_limit  = 0;
    radio_stub_flow_reads     = 0;
    radio_stub_idle_calls     = 0;
    radio_stub_poll_calls     = 0;
    radio_stub_swrscan_updates = 0;
    radio_stub_dsp_calls      = 0;
    radio_stub_idle_marks_n   = 0;
    radio_stub_vol            = 0;
    radio_stub_mode           = 0;
    radio_stub_subject_set_last = 0;
    radio_stub_subject_set_calls = 0;
    radio_stub_rxvol_calls    = 0;
    radio_stub_rxvol_last     = 0;
    radio_stub_bt_muted       = false;
}

/* --- clock ---------------------------------------------------------------- */

static uint64_t monotonic_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t) ts.tv_sec * 1000u + (uint64_t) ts.tv_nsec / 1000000u;
}

uint64_t get_time(void) {
    if (radio_stub_clock == RADIO_STUB_CLOCK_MONOTONIC) {
        return monotonic_ms();
    }

    return radio_stub_fake_ms;
}

/* usleep() is overridden for the whole test binary. The ROM loop only ever asks
 * for 15000 us (issue #219); any other value is recorded so a regression that
 * changes the constant is visible. In FAKE mode the request only advances the
 * fake clock 1:1 (us -> ms), which keeps the test deterministic and instant; in
 * MONOTONIC mode (benchmark) the real syscall runs. */
int usleep(useconds_t usec) {
    if (usec != 15000u) {
        radio_stub_saw_bad_sleep = true;
    }

    radio_stub_sleep_calls++;
    radio_stub_sleep_us_total += usec;

    if (radio_stub_clock == RADIO_STUB_CLOCK_FAKE) {
        radio_stub_fake_ms += usec / 1000u;
    } else {
        struct timespec req = {
            .tv_sec  = (time_t) (usec / 1000000u),
            .tv_nsec = (long) (usec % 1000000u) * 1000L,
        };
        nanosleep(&req, NULL);
    }

    if (radio_stub_exit_on_stop && radio_stub_stop) {
        pthread_exit(NULL);
    }

    if (radio_stub_jmp_armed && radio_stub_sleep_limit > 0 &&
        radio_stub_sleep_calls >= radio_stub_sleep_limit) {
        longjmp(radio_stub_jmp, 1);
    }

    return 0;
}

/* --- radio flow / control ------------------------------------------------- */

bool x6200_flow_read(x6200_flow_t *pack) {
    (void) pack;

    int idx = (int) radio_stub_flow_reads;
    bool ok;

    if (radio_stub_flow_pattern != NULL && radio_stub_flow_pattern_len > 0) {
        ok = radio_stub_flow_pattern[idx % radio_stub_flow_pattern_len];
    } else {
        ok = radio_stub_flow_ok;
    }

    radio_stub_flow_reads++;

    if (radio_stub_exit_on_stop && radio_stub_stop) {
        pthread_exit(NULL);
    }

    if (radio_stub_jmp_armed && radio_stub_flow_read_limit > 0 &&
        (int) radio_stub_flow_reads >= radio_stub_flow_read_limit) {
        longjmp(radio_stub_jmp, 1);
    }

    return ok;
}

bool x6200_flow_restart(void) {
    return true;
}

bool x6200_flow_init(void) {
    return true;
}

void x6200_control_idle(void) {
    radio_stub_idle_calls++;

    if (radio_stub_idle_marks_n < RADIO_STUB_IDLE_MARKS) {
        radio_stub_idle_marks[radio_stub_idle_marks_n++] = radio_stub_sleep_calls;
    }
}

void x6200_control_atu_tune(bool on) {
    (void) on;
}

bool x6200_control_cmd(x6200_cmd_enum_t cmd, uint32_t arg) {
    (void) cmd; (void) arg;

    return true;
}

void x6200_control_poweroff(void) {
}

/* --- radio_tick dependencies ---------------------------------------------- */

void clock_update_power(float ext, float bat, uint8_t cap, bool charge) {
    (void) ext; (void) bat; (void) cap; (void) charge;
}

bool scheduler_put(scheduler_fn_t fn, void *arg, size_t arg_size) {
    (void) fn; (void) arg; (void) arg_size;

    return true;
}

void msg_schedule_text_fmt(const char *fmt, ...) {
    (void) fmt;
}

void msg_update_text_fmt(const char *fmt, ...) {
    (void) fmt;
}

void dsp_samples(float *buf_samples, uint16_t size, bool tx, int16_t dbm) {
    (void) buf_samples; (void) size; (void) tx; (void) dbm;
    radio_stub_dsp_calls++;
}

void dsp_reset(void) {
}

void tx_info_update(float pwr, float vswr, float alc) {
    (void) pwr; (void) vswr; (void) alc;
}

void dialog_swrscan_update(float vswr) {
    (void) vswr;
    radio_stub_swrscan_updates++;
}

void dialog_swrscan_poll(void) {
    radio_stub_poll_calls++;
}

void event_send(lv_obj_t *obj, lv_event_code_t event_code, void *param) {
    (void) obj; (void) event_code; (void) param;
}

void hkey_put(uint32_t key) {
    (void) key;
}

int cfg_atu_save_network(uint32_t value) {
    (void) value;

    return 0;
}

void lv_msg_send(uint32_t msg_id, const void *payload) {
    (void) msg_id; (void) payload;
}

void gpio_safe_set(int pin, int value) {
    (void) pin; (void) value;
}

void btaudio_headphone_volume(int pct) {
    (void) pct;
}

bool btaudio_speaker_muted(void) {
    return radio_stub_bt_muted;
}

void x6200_control_rxvol_set(uint8_t vol) {
    radio_stub_rxvol_calls++;
    radio_stub_rxvol_last = vol;
}

void voice_say_text_fmt(const char *fmt, ...) {
    (void) fmt;
}

int32_t limit(int32_t x, int32_t min, int32_t max) {
    if (x < min) { return min; }
    if (x > max) { return max; }

    return x;
}

int32_t align_int(int32_t x, uint16_t step) {
    if (step == 0) { return x; }

    return (x / (int32_t) step) * (int32_t) step;
}

int32_t cfg_transverter_get_shift(int32_t freq) {
    (void) freq;

    return 0;
}

bool cb_tx_allowed(int32_t freq, int tx_scope) {
    (void) freq; (void) tx_scope;

    return true;
}

void sql_policy_apply(x6200_mode_t mode, uint8_t level) {
    (void) mode; (void) level;
}

void lastbreath_mark_clean_now(void) {
}

/* --- Subject observers used by radio_tick -------------------------------- */

/* El camino del volumen (#244) lee dos Subjects: se distinguen por puntero y el resto sigue
 * devolviendo 0, que es lo que esperan los casos del #219. */
int32_t subject_get_int(Subject *subj) {
    if (subj != NULL && subj == cfg.vol.val) {
        return radio_stub_vol;
    }

    if (subj != NULL && subj == cfg_cur.mode) {
        return radio_stub_mode;
    }

    return 0;
}

void subject_set_int(Subject *subj, int32_t val) {
    (void) subj;

    radio_stub_subject_set_last = val;
    radio_stub_subject_set_calls++;
}

float subject_get_float(Subject *subj) {
    (void) subj;

    return 0.0f;
}

void subject_set_float(Subject *subj, float val) {
    (void) subj; (void) val;
}

/* --- visual policy -------------------------------------------------------- */

void visual_policy_init(visual_policy_t *state) {
    (void) state;
}

visual_override_t visual_policy_override_from_param(uint8_t value) {
    (void) value;

    return vp_override_auto();
}

visual_policy_result_t visual_policy_eval(visual_policy_t *state,
                                          uint16_t vbat_mv, uint16_t vext_mv,
                                          uint8_t batcap, bool charging,
                                          uint32_t last_valid_ms, uint32_t now_ms,
                                          visual_override_t override) {
    (void) state; (void) vbat_mv; (void) vext_mv; (void) batcap; (void) charging;
    (void) last_valid_ms; (void) now_ms; (void) override;

    visual_policy_result_t result;

    memset(&result, 0, sizeof(result));

    return result;
}

bool visual_policy_low_battery_warn(uint16_t vbat_mv, bool charging, bool was_warned) {
    (void) vbat_mv; (void) charging; (void) was_warned;

    return false;
}

void visual_policy_waterfall_apply(visual_profile_t profile, bool user_battery_mode) {
    (void) profile; (void) user_battery_mode;
}

void visual_policy_spectrum_apply(visual_profile_t profile) {
    (void) profile;
}

void visual_policy_brightness_apply(visual_profile_t profile, int16_t user_setting) {
    (void) profile; (void) user_setting;
}

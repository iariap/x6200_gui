/*
 * Test hooks for the native CW keyboard dialog idempotence test (issue #217).
 *
 * The counts are the measurement: every lv_label_set_text/_fmt and every
 * lv_obj_set_style_* is a call that makes LVGL invalidate the object, and the
 * dialog's 100 ms timer used to make four/four of them per tick even when
 * nothing changed.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Full reset: counters, object/label registry and the stubbed firmware state.
 * Call it at the start of every test case. */
void cwkbd_stub_reset(void);

/* Counters only: keeps the objects the dialog built. Use it right after
 * construct() to measure the steady state. */
void cwkbd_stub_counters_reset(void);

int cwkbd_stub_label_writes(void);    /* lv_label_set_text / _fmt calls */
int cwkbd_stub_style_writes(void);    /* lv_obj_set_style_* calls */
int cwkbd_stub_encoder_sends(void);   /* cw_encoder_send calls */
int cwkbd_stub_encoder_stops(void);   /* cw_encoder_stop calls */
int cwkbd_stub_remote_events(void);   /* remote_event calls */

const char *cwkbd_stub_last_sent(void);   /* text of the last cw_encoder_send */

/* Drives the dialog's own 100 ms timer callback, exactly as LVGL would. */
void cwkbd_stub_timer_tick(void);
int  cwkbd_stub_timer_active(void);

/* Firmware state the dialog reads. */
void cwkbd_stub_set_mode(int32_t mode);          /* x6100_mode_* value */
void cwkbd_stub_set_wpm(int32_t wpm);
void cwkbd_stub_set_keyboard_ready(bool ready);
void cwkbd_stub_set_encoder(int state, int sent);   /* cw_encoder_state_t, sent chars */
void cwkbd_stub_set_theme_gen(uint32_t gen);        /* changes every theme_hex() value */
int  cwkbd_stub_cw_decoder(void);                   /* cfg.cw_decoder value */

/* Labels in creation order (construct_cb(): head, head_right, log_lbl,
 * state_lbl, hint -> 0..4). NULL when there is no such label. */
int         cwkbd_stub_label_count(void);
const char *cwkbd_stub_label_text(int i);

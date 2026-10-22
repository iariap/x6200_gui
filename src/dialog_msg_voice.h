/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "dialog.h"

#include "buttons.h"

#include "lvgl/lvgl.h"

typedef enum {
    MSG_VOICE_OFF = 0,
    MSG_VOICE_RECORD,
    MSG_VOICE_PLAY
} msg_voice_state_t;

extern dialog_t *dialog_msg_voice;

msg_voice_state_t dialog_msg_voice_get_state();
void dialog_msg_voice_put_audio_samples(size_t nsamples, int16_t *samples);

/* Named messages, as the Field Ops app uses them: record "CQ SOTA", check that
 * it exists, and send it in SSB. The files are WAV in /mnt/msg and show up in
 * the voice message dialog (long-press action "MSG") as any other one. */
bool msg_voice_rec_toggle(const char *name);
bool msg_voice_exists(const char *name);
bool msg_voice_send_toggle(const char *name);

/* Stops a recording that ran past the limit; called from the main loop and
 * when an app closes, so a recording is never left open */
void msg_voice_poll(bool closing);

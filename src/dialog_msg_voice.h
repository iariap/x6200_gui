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

/* The X6100 fork exposes the voice messages to the Field Ops app. That refactor
 * is not ported, so those buttons are inert: the voice message dialog itself
 * (long-press action "MSG") works as in this GUI. */
bool msg_voice_rec_toggle(const char *name);
bool msg_voice_exists(const char *name);
bool msg_voice_send_toggle(const char *name);

/* Inert as well: this GUI has no periodic voice poll */
void msg_voice_poll(bool closing);

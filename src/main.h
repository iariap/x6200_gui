/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "rotary.h"
#include "encoder.h"

#define VERSION "v0.1.2-lu2dsx"

extern rotary_t     *vol;
extern encoder_t    *mfk;
extern rotary_t     *mfk_inner;

/* Apagado limpio del registro de ultima respiracion (issue #121): lo llama
 * radio_poweroff() antes de apagar el equipo. */
void lastbreath_mark_clean_now(void);

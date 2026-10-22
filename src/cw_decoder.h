/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>

typedef struct {
    char    *morse;
    char    *character;
} cw_characters_t;

extern cw_characters_t cw_characters[];

void cw_decoder_init();
void cw_decoder_signal(bool on, float ms);

/* La velocidad que el decoder mide de la pina (PARIS), en WPM. 0 mientras no
 * haya medido un par punto/raya: el que llama decide el respaldo. */
int cw_decoder_get_wpm(void);

/* Where the decoded text goes: the panel by default, the CW trainer when it sets
 * a sink (docs/NEW_FEATURES.md) */
void cw_decoder_set_sink(void (*text)(const char *), void (*wpm)(int));
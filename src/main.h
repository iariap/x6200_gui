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

/* Version del proyecto (v0.1.2 = 1.0.7 BASE) + sufijo de la ROM de LU2DSX.
 * Sincronizacion trunk->main 2026-10-22: upstream subio v0.1.1 -> v0.1.2 en
 * este mismo archivo; el fork ya usaba v0.1.2 y agrega el sufijo que lo
 * identifica como ROM. Se conserva la cadena del fork (identidad de build,
 * ver tests/check_build_identity.py). */
#define VERSION "v0.1.2-lu2dsx"

extern rotary_t     *vol;
extern encoder_t    *mfk;
extern rotary_t     *mfk_inner;

/* Apagado limpio del registro de ultima respiracion (issue #121): lo llama
 * radio_poweroff() antes de apagar el equipo. */
void lastbreath_mark_clean_now(void);

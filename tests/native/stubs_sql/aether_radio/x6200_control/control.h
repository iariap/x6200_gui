/*
 * Stub de la lib del control para el test nativo del squelch (issue #166).
 *
 * Solo declara lo que usa src/sql_policy.c: el modo y los tres setters del
 * squelch. Las implementaciones de tests/native/sql_policy_stubs.c registran
 * cada llamada para poder afirmar el orden y la cantidad.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    x6200_mode_lsb = 0,
    x6200_mode_lsb_dig = 1,
    x6200_mode_usb = 2,
    x6200_mode_usb_dig = 3,
    x6200_mode_cw = 4,
    x6200_mode_cwr = 5,
    x6200_mode_am = 6,
    x6200_mode_sam = 7,
    x6200_mode_nfm = 8,
    x6200_mode_wfm = 9,
} x6200_mode_t;

void x6200_control_sql_set(uint8_t sql);
void x6200_control_sql_fm_set(uint8_t sql);
void x6200_control_sql_enable_set(bool on);

/*
 * Stub de la lib del control para el test nativo del editor DFL (issue #173,
 * PR 2).
 *
 * Solo declara el enum de modos que usa src/dfl_model.h (y src/cfg/mode.h). Los
 * valores son los de la lib real (aether-x6200-control) y los mismos del stub
 * del test del squelch (tests/native/stubs_sql).
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

typedef enum {
    x6200_agc_off = 0,
    x6200_agc_slow = 1,
    x6200_agc_fast = 2,
    x6200_agc_auto = 3
} x6200_agc_t;

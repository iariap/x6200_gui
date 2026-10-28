/*
 * Stub de radio.h para el banco: src/audio_enhancement.c solo necesita
 * x6200_mode_t (comparaciones con x6200_mode_cw / x6200_mode_cwr en
 * audio_enh_apply_mode_preset). Los valores son los del SDK real,
 * tools/ui_harness/third_party/aether_radio/x6200_control/low/control.h:88-100.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

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

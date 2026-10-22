/*
 * radio.h stub for the native cw_tune_ui test (issue #131): only the mode enum
 * the visibility observer compares against. The real params/params.h pulls
 * src/radio.h in; the stub keeps the same shape.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef enum {
    x6200_mode_lsb     = 0,
    x6200_mode_lsb_dig = 1,
    x6200_mode_usb     = 2,
    x6200_mode_usb_dig = 3,
    x6200_mode_cw      = 4,
    x6200_mode_cwr     = 5,
    x6200_mode_am      = 6,
    x6200_mode_sam     = 7,
    x6200_mode_nfm     = 8,
    x6200_mode_wfm     = 9,
} x6200_mode_t;

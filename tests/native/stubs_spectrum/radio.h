/*
 * radio.h stub for the native spectrum test (issue #149): the mode enum and the
 * DNF mode are the two radio symbols src/spectrum.c reads. The values are
 * copied verbatim from the aether-x6200-control SDK header Buildroot vendors
 * (low/control.h), the same reference the ui_harness keeps in
 * tools/ui_harness/third_party/.
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

typedef enum {
    x6200_dnf_off = 0,
    x6200_dnf_manual,
    x6200_dnf_auto,
} x6200_dnf_mode_t;

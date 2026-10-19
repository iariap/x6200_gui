/*
 * flow.h stub for the native hkey test (issue #131).
 *
 * Values copied verbatim from the aether-x6200-control SDK header
 * (include/aether_radio/x6200_control/low/flow.h, the one Buildroot vendors):
 * they are the bytes the BASE puts in `pack->hkey`. The host has no aether SDK,
 * so the test pins them here; a shift in the SDK would show up as a different
 * key in the radio, and this table is the reference the test reads.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#define X6200_HKEY_SPCH     0xC1
#define X6200_HKEY_TUNER    0xC2
#define X6200_HKEY_XFC      0xC4
#define X6200_HKEY_UP       0x84
#define X6200_HKEY_DOWN     0x04
#define X6200_HKEY_VM       0x81
#define X6200_HKEY_NW       0x82
#define X6200_HKEY_F1       0x48
#define X6200_HKEY_F2       0x44
#define X6200_HKEY_1        0xE1
#define X6200_HKEY_2        0xE2
#define X6200_HKEY_3        0xE4
#define X6200_HKEY_4        0x61
#define X6200_HKEY_5        0x62
#define X6200_HKEY_6        0x64
#define X6200_HKEY_7        0xA1
#define X6200_HKEY_8        0xA2
#define X6200_HKEY_9        0xA4
#define X6200_HKEY_DOT      0x21
#define X6200_HKEY_0        0x22
#define X6200_HKEY_CE       0x24
#define X6200_HKEY_MODE     0xE8
#define X6200_HKEY_FIL      0x68
#define X6200_HKEY_GENE     0xA8
#define X6200_HKEY_FINP     0x28

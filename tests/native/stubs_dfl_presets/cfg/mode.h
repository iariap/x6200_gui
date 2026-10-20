/*
 * Stub de src/cfg/mode.h para el test de presets del DFL (issue #183). Solo los
 * dos setters vigentes y MAX_FILTER_FREQ; el enum de modos sale del stub de la
 * lib de control (tests/native/stubs_dfl).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include <aether_radio/x6200_control/control.h>

#define MAX_FILTER_FREQ 10000

int32_t cfg_mode_set_low_filter(int32_t val);
int32_t cfg_mode_set_high_filter(int32_t val);

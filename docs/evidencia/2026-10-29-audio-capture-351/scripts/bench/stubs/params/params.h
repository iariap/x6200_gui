/*
 * Stub de params para el banco: solo el campo que lee
 * audio_enh_apply_mode_preset() (params.audio_enh_preset.x), con el tipo real
 * de src/params/types.h:41-48.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef struct {
    uint8_t x;
} params_uint8_t;

typedef struct {
    params_uint8_t audio_enh_preset;
} params_t;

extern params_t params;

/*
 * Stub de subjects para el banco: solo subject_get_int(), usado por
 * audio_enh_apply_mode_preset() (que el banco no invoca: cada caso fija el
 * preset efectivo con audio_enh_preset_load()).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef struct Subject Subject;

int32_t subject_get_int(Subject *subj);

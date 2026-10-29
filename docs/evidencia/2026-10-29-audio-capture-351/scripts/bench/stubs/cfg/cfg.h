/*
 * Stub de cfg para el banco: solo el global cfg_cur con el subject de modo,
 * tal como lo lee audio_enh_apply_mode_preset(). Tipos identicos a
 * src/cfg/cfg.h:109-115.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "subjects.h"

typedef struct {
    Subject *mode;
} cfg_cur_t;

extern cfg_cur_t cfg_cur;

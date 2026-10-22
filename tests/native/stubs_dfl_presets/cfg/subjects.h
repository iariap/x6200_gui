/*
 * Stub de src/cfg/subjects.h para el test de presets del DFL (issue #183).
 * El Subject es opaco y su cuerpo lo define tests/native/dfl_presets_stubs.c.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef struct Subject Subject;

int32_t subject_get_int(Subject *subj);
void    subject_set_int(Subject *subj, int32_t val);

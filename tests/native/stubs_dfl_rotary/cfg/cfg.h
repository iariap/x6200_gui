/*
 * Stub minimo de src/cfg/cfg.h para el test del motor de la rueda del editor DFL
 * (tests/native/test_dfl_rotary.c). Solo lo que src/dfl_rotary.c toca: los tres
 * Subjects del filtro, el modo y el Subject. El `struct Subject` se declara aca
 * (opaco) y su cuerpo lo define tests/native/dfl_rotary_stubs.c para que el test
 * pueda leer/escribir su valor; el resto de src/cfg/subjects.h (real, via
 * -I$SRC) aporta las declaraciones de subject_get_int/subject_set_int.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef struct Subject Subject;

int32_t subject_get_int(Subject *subj);
void    subject_set_int(Subject *subj, int32_t val);

typedef struct {
    Subject *mode;
    struct {
        Subject *low;
        Subject *high;
        Subject *bw;
    } filter;
} cfg_cur_t;

extern cfg_cur_t cfg_cur;

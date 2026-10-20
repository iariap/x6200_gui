/*
 * Stub de src/cfg/cfg.h para el test de presets del DFL (issue #183). Solo el
 * estado vivo que toca src/dfl_presets.c: el modo y el filtro (LOW/HIGH/BW y el
 * par real RF). `real` esta para poder afirmar que NADIE lo escribe.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "subjects.h"

typedef struct {
    Subject *mode;
    struct {
        Subject *low;
        Subject *high;
        Subject *bw;
        struct {
            Subject *from;
            Subject *to;
        } real;
    } filter;
} cfg_cur_t;

extern cfg_cur_t cfg_cur;

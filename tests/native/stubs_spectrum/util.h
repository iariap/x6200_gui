/*
 * util.h stub for the native spectrum test (issue #149): the clock the module
 * timestamps peaks with and the one-pole filter it uses for the auto grid.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

uint64_t get_time(void);
void lpf(float *x, float current, float beta, float initial);

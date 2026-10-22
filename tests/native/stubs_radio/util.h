/* Minimal util.h stub for the radio_thread test (issue #219). SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

uint64_t get_time(void);
int32_t  align_int(int32_t x, uint16_t step);
int32_t  limit(int32_t x, int32_t min, int32_t max);
void     sleep_usec(uint32_t msec);

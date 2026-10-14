/*
 * Minimal util.h for the native clock test: only the monotonic time source.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

uint64_t get_time(void);

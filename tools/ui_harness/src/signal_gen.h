/*
 *  Deterministic spectrum/waterfall data for the host UI harness.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

/* Feeds one settled spectrum frame and a column of waterfall frames, all
 * derived from `seed` (no clock, no random device). */
void signal_gen_feed(uint32_t seed);

/*
 *  Deterministic spectrum/waterfall data for the host UI harness.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

/* Waterfall rows the settled feed pushes: the natural drive continues the same
 * column after them (signal_gen_waterfall_row()). */
#define SIGNAL_GEN_WATERFALL_ROWS 200

/* Feeds one settled spectrum frame and a column of waterfall frames, all
 * derived from `seed` (no clock, no random device). */
void signal_gen_feed(uint32_t seed);

/* One spectrum frame of the natural drive: generator row `frame + 1` (row 0 is
 * the settled frame signal_gen_feed() draws), fed through spectrum_data(). */
void signal_gen_spectrum_frame(uint32_t seed, int frame);

/* One waterfall row of the natural drive: generator row `row` after the
 * settled column, fed through waterfall_data(). */
void signal_gen_waterfall_row(uint32_t seed, int row);

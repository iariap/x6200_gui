/*
 *  Render metrics for the host UI harness.
 *
 *  Counts what the display flush sees, so the JSON can report the render cost
 *  of a scenario without any timing: two equivalent runs must report the same
 *  numbers. Wall-clock timing is deliberately not measured here (the future
 *  benchmark milestone does that, with the real monotonic clock).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

void     render_metrics_reset(void);
void     render_metrics_record_flush(uint32_t pixels);

uint32_t render_metrics_flush_count(void);
uint64_t render_metrics_flushed_pixels(void);

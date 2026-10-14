/*
 *  Render metrics for the host UI harness (see render_metrics.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "render_metrics.h"

static uint32_t flush_count;
static uint64_t flushed_pixels;

void render_metrics_reset(void)
{
    flush_count = 0;
    flushed_pixels = 0;
}

void render_metrics_record_flush(uint32_t pixels)
{
    flush_count++;
    flushed_pixels += pixels;
}

uint32_t render_metrics_flush_count(void)
{
    return flush_count;
}

uint64_t render_metrics_flushed_pixels(void)
{
    return flushed_pixels;
}

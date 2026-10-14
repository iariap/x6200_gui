/*
 *  Real-clock render benchmark for the host UI harness.
 *
 *  The virtual clock (virtual_time.c) decides *what* a scenario renders and
 *  must never be polluted by wall-clock values, so that the PNGs stay
 *  bit-identical. This module is the opposite: it reads the real monotonic
 *  clock (CLOCK_MONOTONIC) only to measure how long a frame took on the host.
 *  It never feeds a value back into the GUI, so enabling it cannot change a
 *  single pixel.
 *
 *  Per measured frame it records:
 *    - the total render time of the frame (all redraw passes together);
 *    - the time spent inside the display flush callback;
 *    - the remaining time (layout + draw), derived, never invented;
 *    - the flush count;
 *    - the total redrawn area (sum of the flushed rectangles, repeated
 *      repaints counted again);
 *    - the distinct redrawn pixels (union of the flushed rectangles).
 *
 *  At the end of the run it reads the process memory from /proc (RSS, PSS and
 *  the peak values). Nothing here is a battery or an ARM measurement: it is an
 *  x86 host proxy and is reported as such.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    double   render_ms;        /* wall time of the whole frame */
    double   flush_ms;         /* time inside the flush callback */
    double   draw_ms;          /* render_ms - flush_ms (layout + draw) */
    uint32_t flushes;          /* flush callback invocations */
    uint32_t redrawn_area_px;  /* sum of the flushed rectangle areas */
    uint32_t unique_redrawn_px;/* distinct pixels covered by those rectangles */
} bench_frame_t;

typedef struct {
    uint64_t rss_bytes;        /* VmRSS: resident set size */
    uint64_t pss_bytes;        /* Pss from smaps_rollup (0 when unavailable) */
    uint64_t peak_rss_bytes;   /* VmHWM: peak resident set size */
    uint64_t peak_vm_bytes;    /* VmPeak: peak virtual memory */
} bench_memory_t;

/* Monotonic clock in nanoseconds, independent from the virtual clock. */
uint64_t bench_now_ns(void);

/* Enables/disables the instrumentation. When disabled every hook is a cheap
 * no-op, so the normal (PNG) mode is not slowed down or altered. */
void bench_enable(bool on);
bool bench_enabled(void);

/* Flush callback hooks: call bench_flush_begin() before copying and
 * bench_flush_end() after, with the flushed rectangle (inclusive bounds). */
void bench_flush_begin(void);
void bench_flush_end(int32_t x0, int32_t y0, int32_t x1, int32_t y1);

/* Frame hooks: begin resets the per-frame accumulators, end closes the frame
 * with the measured render time and appends it to the sample list. */
void bench_frame_begin(void);
void bench_frame_end(double render_ms);

size_t               bench_frame_count(void);
const bench_frame_t *bench_frame_at(size_t index);

/* Reads RSS/PSS/peak from /proc; zeroes what the kernel does not expose. */
bench_memory_t bench_read_memory(void);

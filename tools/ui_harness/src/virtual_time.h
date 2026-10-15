/*
 *  Virtual time for the host UI harness.
 *
 *  LVGL's timers, animations and tick advance with lv_tick_inc(); none of them
 *  reads the wall clock. The harness therefore drives them from a counter it
 *  owns: advancing N milliseconds runs lv_tick_inc() and lv_timer_handler() in
 *  fixed slices, with no sleep() anywhere. Two runs that advance the same
 *  amount of virtual time see exactly the same sequence of ticks, so the
 *  rendered state is reproducible.
 *
 *  The real monotonic clock (used by get_time() in the firmware for
 *  measuring durations) is wired to this same counter so that nothing in the
 *  rendered state can depend on how long the process took.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

/* Milliseconds elapsed on the harness clock. Starts at a fixed, non-zero
 * epoch so that "now > timeout" comparisons behave like a running radio. */
uint64_t virtual_time_now_ms(void);

void virtual_time_reset(uint64_t start_ms);

/* Run once per slice of `virtual_time_advance_step()`, after the tick has been
 * incremented and before LVGL's timers run: where the natural bench drives the
 * components' data from (natural_bench.c). */
typedef void (*virtual_time_slice_cb_t)(void *arg);

/* Advance the clock by `ms`, running LVGL's timer handler every `slice_ms`
 * (and flushing the firmware's delayed observers each slice). No sleeps. */
void virtual_time_advance(uint64_t ms, uint32_t slice_ms);

/* Same as virtual_time_advance(), with `on_slice` (when not NULL) called once
 * per slice. No sleeps. */
void virtual_time_advance_step(uint64_t ms, uint32_t slice_ms,
                               virtual_time_slice_cb_t on_slice, void *arg);

/* Advance the clock by `ms` with LVGL's timer handler NOT running: only the
 * tick and `on_slice` move. It is the ROM's main loop *asleep* (src/main.c:
 * `usleep(sleep_time * 1000)` until the next timer it says is due), where the
 * tick thread keeps calling lv_tick_inc() and the DSP thread keeps posting
 * data, but no timer runs. The wakeup profile (wakeup_bench.c) measures the
 * loop with it: running the handler here would run the timers during the sleep
 * and count wakeups that the firmware does not have. */
void virtual_time_advance_idle(uint64_t ms, uint32_t slice_ms,
                               virtual_time_slice_cb_t on_slice, void *arg);

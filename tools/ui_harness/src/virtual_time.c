/*
 *  Virtual time for the host UI harness (see virtual_time.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "virtual_time.h"

#include "lvgl/lvgl.h"
#include "cfg/subjects.h"

static uint64_t now_ms = 1000;

uint64_t virtual_time_now_ms(void)
{
    return now_ms;
}

void virtual_time_reset(uint64_t start_ms)
{
    now_ms = start_ms;
}

void virtual_time_advance(uint64_t ms, uint32_t slice_ms)
{
    if (slice_ms == 0) {
        slice_ms = 1;
    }

    uint64_t end = now_ms + ms;

    while (now_ms < end) {
        uint32_t step = (uint32_t) (end - now_ms);

        if (step > slice_ms) {
            step = slice_ms;
        }
        now_ms += step;
        lv_tick_inc(step);
        lv_timer_handler();
        observer_delayed_notify_all();
    }
}

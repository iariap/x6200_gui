/*
 * Host implementations for the events-queue test (issue #111).
 *
 * The LVGL, keyboard and log symbols src/events.c touches, plus the observation
 * surface the test uses (dispatched events, log lines, a controllable tick).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

/* Resets the queue observation state (dispatched counters, log lines, tick).
 * The queue itself is reset by event_init(). */
void     events_stub_reset(void);

void     events_stub_tick_set(uint32_t ms);

int      events_stub_dispatched(void);
int      events_stub_invalidated(void);
int      events_stub_log_lines(void);
/* Last captured log line, empty string if none. */
const char *events_stub_last_log(void);

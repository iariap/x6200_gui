/*
 * Minimal scheduler stub for the native SWR Scan dialog test (issue #125).
 *
 * The dialog's completion/abort path (detected in the radio thread) hands the
 * LVGL-side teardown to the scheduler. The stub records the pending job so the
 * test can run it on demand (swrscan_stub_run_scheduler), which is how the real
 * main thread would run it.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stddef.h>

typedef void (* scheduler_fn_t)(void *);

void scheduler_put_noargs(scheduler_fn_t fn);

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2024 Georgy Dyuldin aka R2RFE
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef void (* scheduler_fn_t)(void *);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Schedule execution function in main thread.
 *
 * Returns true when the work was queued. A full queue (or a failed argument
 * copy) returns false: the caller can then re-arm its own bookkeeping instead
 * of waiting for a callback that will never run.
 */
bool scheduler_put(scheduler_fn_t fn, void *arg, size_t arg_size);


/**
 * Schedule execution function without arguments in main thread.
 */
bool scheduler_put_noargs(scheduler_fn_t fn);

/**
 * Execute scheduled functions
 */
void scheduler_work();

#ifdef __cplusplus
}
#endif

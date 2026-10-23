/* Minimal scheduler.h stub for the radio_thread test (issue #219). SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef void (*scheduler_fn_t)(void *);

bool scheduler_put(scheduler_fn_t fn, void *arg, size_t arg_size);

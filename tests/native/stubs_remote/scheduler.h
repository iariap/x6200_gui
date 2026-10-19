/* Stub for src/scheduler.h (native remote_server test). */
#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef void (*scheduler_fn_t)(void *);

bool scheduler_put(scheduler_fn_t fn, void *arg, size_t arg_size);

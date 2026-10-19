/*
 * cfg/cfg.h stub for the native cw_tune_ui test (issue #131).
 *
 * src/cw_tune_ui.c reads the current mode through a Subject pointer and
 * subscribes to it with a delayed observer. The stub keeps the same shape with
 * a minimal, test-owned implementation in tests/native/cw_tune_stubs.c: the
 * subject holds an int32 value, set() notifies the non-delayed observers in the
 * calling thread and records the delayed ones (the test can drain them).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "../lvgl/lvgl.h"

typedef struct Subject Subject;
typedef struct ObserverDelayed ObserverDelayed;

typedef void (*cw_tune_observer_fn_t)(Subject *subj, void *user_data);

int32_t           subject_get_int(Subject *subj);
ObserverDelayed  *subject_add_delayed_observer(Subject *subj, cw_tune_observer_fn_t fn, void *user_data);

typedef struct {
    Subject *mode;
} cfg_cur_t;

extern cfg_cur_t cfg_cur;

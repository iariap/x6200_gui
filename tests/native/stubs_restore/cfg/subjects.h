/*
 * Minimal subjects stub for the native dialog restore test (issue #94).
 *
 * The real subject implementation is C++ (src/cfg/subjects.cpp); the test
 * provides a tiny C one (tests/native/restore_stubs.c) with the same API, so
 * src/dialog.c compiles unchanged.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct Subject Subject;

#include "cfg.h"

Subject *subject_create_int(int32_t val);

int32_t subject_get_int(Subject *subj);
void    subject_set_int(Subject *subj, int32_t val);

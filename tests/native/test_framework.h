/*
 * Tiny assertion framework for the native/ARM unit tests (no external deps).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>

extern int tests_run;
extern int tests_failed;

void test_check(bool ok, const char *expr, const char *file, int line);
void test_check_close(float got, float want, float tol, const char *expr, const char *file, int line);

#define CHECK(cond) \
    test_check((cond), #cond, __FILE__, __LINE__)

#define CHECK_CLOSE(got, want, tol) \
    test_check_close((got), (want), (tol), #got " ~= " #want, __FILE__, __LINE__)

/*
 * Runner for the native DFL presets test (tests/native/test_dfl_presets.c).
 * Separate binary: the model (src/dfl_presets.c) and its store
 * (src/dfl_presets_store.c) are compiled against their own cfg/params stubs
 * (tests/native/stubs_dfl_presets) and a real in-memory sqlite3.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_dfl_presets(void);

int main(void) {
    test_dfl_presets();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

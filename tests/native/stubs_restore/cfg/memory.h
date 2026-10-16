/* Minimal cfg/memory stub for the native dialog restore test (issue #94).
 *
 * The SQLite-backed backup (cfg_memory_save/load) is stubbed out here on
 * purpose: the native test has no database, and the fix moves the app backup to
 * the in-memory radio_state_save/restore so it does not depend on SQLite. Kept
 * so the pre-fix src/dialog.c still links, making the test red before the fix.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool cfg_memory_load(int32_t id);
void cfg_memory_save(int32_t id);

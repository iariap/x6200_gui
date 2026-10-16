/* Minimal main_screen stub for the native dialog restore test (issue #94).
 *
 * MEM_BACKUP_ID is kept so the pre-fix src/dialog.c (which used the SQLite slot
 * cfg_memory_save/load(MEM_BACKUP_ID)) still links, making the test red before
 * the fix. The fixed dialog.c no longer uses it.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <stdbool.h>

#define MEM_HKEY_MAX_ID  9
#define MEM_BACKUP_ID    (MEM_HKEY_MAX_ID + 1)

void main_screen_keys_enable(bool enable);

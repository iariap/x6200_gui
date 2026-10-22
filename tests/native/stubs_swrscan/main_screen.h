/*
 * Minimal main_screen stub for the native SWR Scan dialog test (issue #85).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#define MEM_HKEY_MAX_ID 9
#define MEM_BACKUP_ID   (MEM_HKEY_MAX_ID + 1)

void mem_load(uint16_t id);
void mem_save(uint16_t id);

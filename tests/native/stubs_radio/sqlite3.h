/*
 * Minimal sqlite3 stub for the radio_thread test (issue #219).
 *
 * src/cfg/cfg.h declares cfg_init(sqlite3 *) and nothing else the radio module
 * needs; the real sqlite3 header is not linked into the host stage.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef struct sqlite3 sqlite3;

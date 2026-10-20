/*
 * Stub de src/params/db.h para el test de presets del DFL (issue #183):
 * src/dfl_presets_store.c solo toca el handle sqlite3 `db` y sqlite3_*().
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <sqlite3.h>

extern sqlite3 *db;

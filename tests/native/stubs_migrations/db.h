/*
 * Minimal db surface for the native CB-bands migration test (issue #146):
 * src/params/migrations.c only touches the sqlite3 handle `db` (everything else
 * goes through sqlite3_*()). The real db.h pulls in pthread and the params
 * writers of the firmware; none of that is needed here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <sqlite3.h>

extern sqlite3 *db;

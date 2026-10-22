/*
 * Stub symbols for the native CB-bands migration test (issue #146).
 *
 * src/params/migrations.c links against the sqlite3 handle `db` and references
 * theme_offered() from its issue #74 migration (compiled in, but never called by
 * the test). Both live here so the real migrations.c can be staged and linked
 * against a real in-memory database.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <sqlite3.h>
#include <string.h>

#include "theme.h"

/* Defined once for the whole test binary; the test points it at an in-memory DB. */
sqlite3 *db = NULL;

/* The six shipped themes (docs/THEMES.md); the CB migration never asks. */
bool theme_offered(const char *name) {
    static const char *offered[] = { "Aurora", "Cathode", "Redshift",
                                     "Sprint", "Sunlit", "Trail" };
    for (unsigned i = 0; i < sizeof(offered) / sizeof(offered[0]); i++) {
        if (name != NULL && strcmp(name, offered[i]) == 0) {
            return true;
        }
    }
    return false;
}

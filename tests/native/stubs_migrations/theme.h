/*
 * Minimal theme surface for the native CB-bands migration test (issue #146):
 * src/params/migrations.c uses theme_offered() (in the issue #74 migration) and
 * THEME_DEFAULT. The real theme.h pulls in LVGL; neither is needed here, and the
 * migration under test (_4_cb_bands) does not consult them.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>

#define THEME_DEFAULT "Aurora"

bool theme_offered(const char *name);

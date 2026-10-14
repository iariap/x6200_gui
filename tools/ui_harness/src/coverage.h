/*
 *  Component coverage for the host UI harness.
 *
 *  Every scenario must declare, per component, how much of the real firmware
 *  it exercises. `REAL` means the repository's own source is compiled and
 *  drawn; `PARTIAL` means real source with substituted dependencies;
 *  `SYNTHETIC` means a harness-owned stand-in; `UNSUPPORTED` means the
 *  component is not rendered at all. Never claimed richer than it is.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef enum {
    COVERAGE_REAL = 0,
    COVERAGE_PARTIAL,
    COVERAGE_SYNTHETIC,
    COVERAGE_UNSUPPORTED
} coverage_level_t;

typedef struct {
    const char      *component;
    coverage_level_t level;
    const char      *note;
} coverage_entry_t;

/* The entries for the main-screen scenarios. */
const coverage_entry_t *coverage_main_screen(int *count);

/* The entries that match the screen a scenario builds: the main-screen table,
 * or the dialog table when the scenario opens the real About window. */
const coverage_entry_t *coverage_for_screen(const char *screen, int *count);

const char *coverage_level_name(coverage_level_t level);

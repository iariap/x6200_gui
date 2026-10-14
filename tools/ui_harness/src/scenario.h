/*
 *  Scenario files for the host UI harness.
 *
 *  A scenario is a small text file of `key value` lines that fixes the radio
 *  state and names the screen to build. Fixtures live in fixtures/. Nothing is
 *  read from the environment: the same file always yields the same screen.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SCENARIO_NAME_MAX   64
#define SCENARIO_SCREEN_MAX 32

typedef struct {
    char name[SCENARIO_NAME_MAX];
    char screen[SCENARIO_SCREEN_MAX];
    /* Deterministic seed for the synthetic spectrum/waterfall data. */
    uint32_t seed;
} scenario_t;

/* Parses `path`. Unknown keys are an error (a scenario must not silently
 * ignore a value). Returns false and prints the reason to stderr. */
bool scenario_load(const char *path, scenario_t *out);

/* Builds the named screen. Returns false when the name is unknown. */
bool scenario_build(const scenario_t *scenario);

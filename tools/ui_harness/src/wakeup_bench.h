/*
 *  Wakeup profile for the host UI harness (see wakeup_bench.c).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "scenario.h"

/* Enables the instrumentation. When disabled every hook is a no-op, so the
 * plain (PNG) mode, the forced bench and the natural bench are untouched. */
void wakeup_bench_enable(bool on);
bool wakeup_bench_enabled(void);

/* Measures the ROM's main loop (src/main.c) over `ms` of virtual time on the
 * firmware's own timer set: two passes, one with the deterministic data drive
 * on (the radio's steady state) and one with it off (the delimitation control).
 * A zero window means the default. Call it after the screen has settled. */
void wakeup_bench_run(const scenario_t *scenario, uint32_t ms);

/* The `wakeups` block of the JSON report (a complete member, no trailing
 * comma). */
void wakeup_bench_write_json(FILE *f);

uint32_t wakeup_bench_window_ms(void);

/* Los dos numeros que el perfil declara y el gate comprueba contra la fuente: el
 * periodo de refresco con el que corre el lazo (el de la ROM, lv_conf.h) y la
 * cadencia del hilo del tick (src/main.c). */
uint32_t wakeup_bench_refresh_period_ms(void);
uint32_t wakeup_bench_tick_thread_ms(void);

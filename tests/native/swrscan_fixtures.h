/*
 * Loader for the SWR Scan mock fixtures (issue #125, section 7).
 *
 * The owner delivered ready-to-use, deterministic datasets under
 * tests/fixtures/swrscan/ (a separate branch, copied here): a samples table, a
 * per-point expected-aggregate table, a scenario/ground-truth JSON and a
 * command/packet flow with a stale reading after every frequency change. They
 * are the ORACLE of the multipass tests: the tests replay them through the real
 * engine (and the real dialog) and compare against the expected tables instead
 * of inventing curves that would let the code pass.
 *
 * The fixtures are plain CSV with no quoted fields, so this is a small parser,
 * not a CSV library. Nothing here is production code: it lives with the native
 * tests and is never compiled into the ROM.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#define SWR_FX_MAX_SAMPLES 2048
#define SWR_FX_MAX_AGGREGATES 512
#define SWR_FX_MAX_FLOW 1024

/* One row of swr_mock_samples.csv. `raw_tenths` is the detector value in 0.1
 * SWR units; `tenths * 0.1f` is exactly what src/radio.c hands the engine
 * (`pack->vswr * 0.1f`). */
typedef struct {
    char     scenario[64];
    int      pass_index;
    int      order_in_pass;   /* acquisition order inside the pass (odd passes run backward) */
    int      point_index;     /* grid index the sample belongs to */
    uint32_t freq_hz;
    int      raw_tenths;
    int      is_valid;
    uint32_t timestamp_ms;
} swr_fx_sample_t;

/* One row of swr_expected_aggregates.csv: the exact per-point oracle. */
typedef struct {
    char     scenario[64];
    int      point_index;
    uint32_t freq_hz;
    int      valid_samples;
    int      median_tenths;
    int      mad_tenths;
} swr_fx_aggregate_t;

/* One row of swr_mock_delayed_flow.csv. `source_freq_hz` and `stale` are the
 * hidden oracle columns: they are ONLY for the test's assertions and are never
 * handed to the engine (the README is explicit about it). `is_set_freq` marks a
 * set_frequency command; otherwise it is a swr_packet. */
typedef struct {
    int      seq;
    uint32_t ts_ms;
    int      is_set_freq;
    int      pass_index;
    int      point_index;
    uint32_t target_freq_hz;
    uint32_t source_freq_hz;
    int      swr_tenths;      /* -1 when the row is a command */
    int      stale;
    int      tx_flag;
} swr_fx_flow_event_t;

/* The directory the fixtures are read from: $X6200_SWRSCAN_FIXTURES, or
 * tests/fixtures/swrscan relative to the working directory. */
const char *swr_fx_dir(void);

/* Loads each table into `out` (at most `max` rows). Returns the number of rows,
 * or -1 when the file cannot be opened or a row is malformed (the test must not
 * silently pass with no data). */
int swr_fx_load_samples(swr_fx_sample_t *out, int max);
int swr_fx_load_aggregates(swr_fx_aggregate_t *out, int max);
int swr_fx_load_flow(swr_fx_flow_event_t *out, int max);

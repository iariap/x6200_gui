/*
 * Shared stubs and hooks for the radio_thread tests (issue #219).
 *
 * src/radio.c is compiled into the test binaries by inclusion (see
 * tests/native/test_radio_thread.c), so the static radio_thread() and its
 * private state are reachable. Everything src/radio.c calls that is not the
 * loop itself is stubbed here and wired to observable counters so the tests can
 * assert on the polling cadence and on the recovery of the flow path.
 *
 * Two clocks: RADIO_STUB_CLOCK_FAKE is a plain counter the deterministic test
 * steps 15000 us at a time (the real sleep is skipped, so the test is instant);
 * RADIO_STUB_CLOCK_MONOTONIC reads the host monotonic clock and runs the real
 * sleep, for the before/after benchmark.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RADIO_STUB_CLOCK_FAKE = 0,
    RADIO_STUB_CLOCK_MONOTONIC,
} radio_stub_clock_t;

extern radio_stub_clock_t radio_stub_clock;
extern uint64_t           radio_stub_fake_ms;

/* Sombra de la lib para el detector de dueno extranjero del coordinador #367. */
extern uint32_t           radio_stub_shadow_reg12;

/* usleep() hooks. */
extern int      radio_stub_sleep_calls;
extern uint64_t radio_stub_sleep_us_total;
extern bool     radio_stub_saw_bad_sleep;   /* a sleep other than 15000 us */
extern int      radio_stub_sleep_limit;     /* >0: longjmp after that many sleeps */
extern jmp_buf  radio_stub_jmp;
extern bool     radio_stub_jmp_armed;       /* guard so a stray sleep never longjmps */
extern volatile bool radio_stub_stop;       /* benchmark: leave the thread */
extern bool     radio_stub_exit_on_stop;

/* Flow / radio control hooks. */
extern bool     radio_stub_flow_ok;
extern const bool *radio_stub_flow_pattern;   /* optional per-read script */
extern int      radio_stub_flow_pattern_len;
extern int      radio_stub_flow_read_limit;   /* >0: longjmp after that many reads */
extern uint64_t radio_stub_flow_reads;
extern int      radio_stub_idle_calls;
extern int      radio_stub_poll_calls;
extern int      radio_stub_swrscan_updates;
extern int      radio_stub_dsp_calls;

/* Sleep count at each x6200_control_idle() call (the IDLE_TIMEOUT cadence). */
#define RADIO_STUB_IDLE_MARKS 32
extern int      radio_stub_idle_marks[RADIO_STUB_IDLE_MARKS];
extern int      radio_stub_idle_marks_n;

/* --- audio de recepcion (issue #244) -------------------------------------- */

/* radio.c lee dos Subjects con subject_get_int() en el camino del volumen y los distingue por
 * puntero: el test deja cfg.vol.val y cfg_cur.mode en valores propios y estos son los valores
 * que devuelve el stub (cualquier otro Subject sigue devolviendo 0). */
extern int32_t  radio_stub_vol;                /* subject_get_int(cfg.vol.val) */
extern int32_t  radio_stub_mode;               /* subject_get_int(cfg_cur.mode) */
extern int32_t  radio_stub_subject_set_last;   /* ultimo valor de subject_set_int() */
extern int      radio_stub_subject_set_calls;

/* Registro 14 del BASE (nivel de audio de recepcion) y mute por auriculares A2DP. */
extern int      radio_stub_rxvol_calls;
extern uint8_t  radio_stub_rxvol_last;
extern bool     radio_stub_bt_muted;

void radio_stub_reset(void);

/*
 * Test-facing surface of the stubs used by the native spectrum test (issue
 * #149). See tests/native/spectrum_stubs.c for the implementation and
 * tests/native/test_spectrum.c for the cases.
 *
 * The stub records every draw primitive the real spectrum callback issues
 * (colour + geometry) and the thread that issued it, and it can stop the draw
 * at an exact line so the test interleaves the producer deterministically
 * instead of hoping for a timing window.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cfg/cfg.h"

/* Geometry of the fake widget: 800x100 at (0,0), like the real spectrum. */
#define SPECTRUM_STUB_W 800
#define SPECTRUM_STUB_H 100

/* theme_hex() values the stub hands out per role, so a recorded line says
 * whether it belongs to the main trace or to the peak trace. */
#define SPECTRUM_STUB_COLOR_MAIN 0x0000DDu
#define SPECTRUM_STUB_COLOR_PEAK 0x00DD00u
#define SPECTRUM_STUB_COLOR_TX   0xDD0000u

#define SPECTRUM_STUB_MAX_LINES 8192
#define SPECTRUM_STUB_MAX_RECTS 16

typedef struct {
    uint32_t color;
    int16_t  x1;
    int16_t  y1;
    int16_t  x2;
    int16_t  y2;
} spectrum_stub_line_t;

typedef struct {
    uint32_t bg;
    int16_t  x1;
    int16_t  y1;
    int16_t  x2;
    int16_t  y2;
} spectrum_stub_rect_t;

extern spectrum_stub_line_t spectrum_stub_lines[SPECTRUM_STUB_MAX_LINES];
extern unsigned             spectrum_stub_lines_n;
extern spectrum_stub_rect_t spectrum_stub_rects[SPECTRUM_STUB_MAX_RECTS];
extern unsigned             spectrum_stub_rects_n;

/* LVGL entry points that ran in a thread other than the declared GUI one. */
extern unsigned spectrum_stub_foreign_lvgl;

/* lv_obj_invalidate() calls (the refresh the scheduler runs on the GUI thread). */
extern unsigned spectrum_stub_invalidates;

/* Declares the calling thread as the LVGL (GUI) thread. */
void spectrum_stub_gui_thread(void);

/* Builds the cfg/cfg_cur subjects once, resets the recorded state, the pause
 * protocol and the fake scheduler, and sets the params defaults. */
void spectrum_stub_reset(void);

/* Drops the recorded lines/rects (used between draws of a stress loop). */
void spectrum_stub_clear_records(void);

/* Runs the draw callback spectrum_init() registered (LV_EVENT_DRAW_MAIN_END). */
void spectrum_stub_draw(void);

/* --- deterministic pause of the draw -------------------------------------- */

/* The draw stops when it is about to record line `index` (0-based, counting
 * every lv_draw_line) and waits for spectrum_stub_resume(). `index` < 0 disarms
 * the pause. */
void spectrum_stub_pause_at(int index);

/* Waits (bounded) until the draw reached the armed line. */
bool spectrum_stub_wait_paused(unsigned timeout_ms);

/* Lets the paused draw continue. */
void spectrum_stub_resume(void);

/* True when a pause hit its timeout: the draw was never released in time (the
 * interleaving could not happen, e.g. the module held a lock across the draw). */
bool spectrum_stub_pause_timed_out(void);

/* --- fake scheduler ------------------------------------------------------- */

unsigned spectrum_stub_sched_pending(void);
unsigned spectrum_stub_sched_peak(void);
void     spectrum_stub_sched_drain(void);

/* Makes the next `times` scheduler_put* calls fail (queue full), so the test can
 * pin that the producer re-arms instead of losing its refresh. */
void spectrum_stub_sched_reject_next(unsigned times);

/* --- subjects ------------------------------------------------------------- */

/* Sets the subject value and notifies its observers (in the calling thread,
 * like the firmware's SubjectT::set, and only when the value changes). */
void spectrum_stub_subject_set(Subject *subj, int32_t value);

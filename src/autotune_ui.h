/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Fine-tuning assistant UI (issue #100)
 *
 *  Copyright (c) 2024 LU2DSX
 *
 *  Drives the deterministic core (src/autotune.c) over the live radio: it
 *  walks the probe plan on an LVGL timer, retuning the dial at every probe so
 *  the operator sees the search (the frequency row moves), feeds the S-meter
 *  level of each probe and, when done, moves the dial to the best spot - or
 *  restores the start frequency when nobody beats it. RX only: it never keys
 *  the radio.
 *
 *  Restore discipline (the house pattern of issue #102,
 *  radio_state_save/radio_state_restore in src/radio.c): the assistant is an
 *  action, not an app, so it does not go through the app frame's snapshot.
 *  It saves its own anchor - the dial at start - and puts it back
 *  deterministically whenever the search does not beat it or is abandoned
 *  (the operator presses the button again, or opens an app / a dialog, which
 *  would otherwise move the dial behind the window).
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Plan a scan around the current dial and start it (no-op if already running). */
void autotune_ui_start(void);

/* Start a scan, or cancel the running one and restore the dial. */
void autotune_ui_toggle(void);

/* Stop the scan and put the dial back at the start frequency. */
void autotune_ui_cancel(void);

/* True while a scan is running (used by the harness fixture to wait it out). */
bool autotune_ui_running(void);

#ifdef __cplusplus
}
#endif

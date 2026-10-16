/*
 * Stubs for the native CW encoder test (tests/native/test_cw_encoder.c,
 * issue #116). The REAL src/cw_encoder.c is staged next to these (the same
 * pattern as the clock/backlight/digital-modes/SWR-Scan tests); only its
 * collaborators are faked here.
 *
 * The keyer's crash path is `cw_encoder_send(NULL)` -> `strdup(NULL)`, which
 * faults inside libc's strlen (fallo=(nil)). The stub records what the keyer
 * asks of the radio (the morse key toggles) and of the message sink, so the
 * test can tell a real send from the NULL no-op.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>
#include <stdint.h>

#include "cfg/cfg_api.h"
#include "params/params.h"
#include "radio.h"
#include "msg.h"
#include "buttons.h"
#include "cw_decoder.h"

/* --- recorded calls, read by the test -------------------------------------- */

static int stub_key_toggles;
static int stub_msg_updates;

int cwenc_stub_key_toggles(void) { return stub_key_toggles; }
int cwenc_stub_msg_updates(void) { return stub_msg_updates; }

void cwenc_stub_reset(void) {
    stub_key_toggles = 0;
    stub_msg_updates = 0;
}

/* --- cfg / params ---------------------------------------------------------- */

cfg_t    cfg;
params_t params;

int32_t subject_get_int(Subject *subj)   { (void) subj; return 20; }   /* WPM */
float   subject_get_float(Subject *subj) { (void) subj; return 3.0f; } /* ratio */

/* --- radio / msg / buttons ------------------------------------------------- */

void radio_set_morse_key(bool on) { (void) on; stub_key_toggles++; }

void msg_update_text_fmt(const char *fmt, ...) { (void) fmt; stub_msg_updates++; }

buttons_page_t buttons_page_msg_cw_1;

void buttons_unload_page(void) { }
void buttons_load_page(buttons_page_t *page) { (void) page; }

/* --- the CW character table (normally in src/cw_decoder.c) -----------------
 * get_morse() walks it until character->morse is NULL, so the array has to be
 * NULL-terminated. Only "E" is used by the test's positive control. */
cw_characters_t cw_characters[] = {
    { ".", "E" },
    { NULL, NULL }
};

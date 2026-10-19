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
#include <pthread.h>

#include "cfg/cfg_api.h"
#include "params/params.h"
#include "radio.h"
#include "msg.h"
#include "buttons.h"
#include "cw_decoder.h"

/* --- recorded calls, read by the test -------------------------------------- */

static int stub_key_toggles;
static int stub_msg_updates;
static int stub_voice_announcements;

/* Frecuencia del dial y politica del interlock de CB (issue #160): el test los
 * mueve para recorrer la matriz de cb_tx_allowed() a traves del keyer. */
static int32_t stub_freq = 27185000;   /* default: dentro del SBC argentino */

/* Dummy Subject handle: the test only ever compares its address, never
 * dereferences it (subject_get_int is faked here). */
static int32_t stub_subj_fg_freq;

int cwenc_stub_key_toggles(void) { return stub_key_toggles; }
int cwenc_stub_msg_updates(void) { return stub_msg_updates; }
int cwenc_stub_voice_announcements(void) { return stub_voice_announcements; }

void cwenc_stub_reset(void) {
    stub_key_toggles = 0;
    stub_msg_updates = 0;
    stub_voice_announcements = 0;
    stub_freq  = 27185000;
    params.cb_tx_scope.x = 0;
}

void cwenc_stub_set_freq(int32_t freq) { stub_freq = freq; }
void cwenc_stub_set_scope(int scope)   { params.cb_tx_scope.x = (uint8_t) scope; }

/* --- cfg / params ---------------------------------------------------------- */

cfg_t    cfg;
cfg_cur_t cfg_cur = { .fg_freq = (Subject *) &stub_subj_fg_freq };
params_t params;

int32_t subject_get_int(Subject *subj) {
    if (subj == cfg_cur.fg_freq) {
        return stub_freq;      /* dial */
    }
    return 20;                 /* WPM */
}
float   subject_get_float(Subject *subj) { (void) subj; return 3.0f; } /* ratio */

/* --- radio / msg / buttons ------------------------------------------------- */

void radio_set_morse_key(bool on) { (void) on; stub_key_toggles++; }

void msg_update_text_fmt(const char *fmt, ...) { (void) fmt; stub_msg_updates++; }

void voice_say_text_fmt(const char *fmt, ...) { (void) fmt; stub_voice_announcements++; }

buttons_page_t buttons_page_msg_cw_1;

void buttons_unload_page(void) { }
void buttons_load_page(buttons_page_t *page) { (void) page; }

/* --- threads: none in the test process --------------------------------------
 * The real keyer runs endecode_thread() in its own pthread (joinable, cancelled and
 * joined by cw_encoder_stop()). The test stub runs the routine SYNCHRONOUSLY and
 * makes join/cancel/detach no-ops, so the test process never has a second thread.
 * Reason: the CI runner's GCC-13 ASan runtime fails its own CHECK inside
 * AsanThread::Destroy() (asan_thread.cpp:367, the alternate-signal-stack walk) when
 * a thread exits; a thread-free test cannot reach that path. The keyer's observable
 * behaviour is the same -- the message is keyed and finishes before
 * cw_encoder_send() returns. No production attribute (stack size, detached) is
 * touched: only the test's view of pthread_create. */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    (void) thread;
    (void) attr;
    start_routine(arg);
    return 0;
}

int pthread_join(pthread_t thread, void **retval) { (void) thread; (void) retval; return 0; }
int pthread_cancel(pthread_t thread)              { (void) thread; return 0; }
int pthread_detach(pthread_t thread)              { (void) thread; return 0; }

/* --- the CW character table (normally in src/cw_decoder.c) -----------------
 * get_morse() walks it until character->morse is NULL, so the array has to be
 * NULL-terminated. Only "E" is used by the test's positive control. */
cw_characters_t cw_characters[] = {
    { ".", "E" },
    { NULL, NULL }
};

/*
 * Unit test: the CW decoder's element buffer is bounded (issue #51).
 *
 * The decoder collects the dots and dashes of the character it is receiving in a
 * fixed `elements[]` buffer and strcat()s one character per key-up. A run of key
 * events with no inter-character space - a stuck signal, or the corrupted audio
 * the risk report describes - used to grow that buffer past its size and glibc's
 * fortify check aborted the process ("*** buffer overflow detected ***"), which
 * on the radio is the GUI restarting every ~30 s while the CW Trainer keeps the
 * decoder fed (issue #51, root cause in #19).
 *
 * This drives cw_decoder_signal() with such a run (300 elements over the 128
 * element buffer) and checks that the process survives and the decoder still
 * decodes a well-formed character afterwards. On the unbounded code the very
 * first CHECK is never reached: the strcat overrun aborts inside libc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "cw_decoder.h"
#include "test_framework.h"

/* The CW Trainer hands the decoder a sink; the recorder plays that role. */
static char   got[4096];
static size_t got_n;
static int    got_calls;

static void sink_text(const char *t) {
    size_t l = strlen(t);

    if (got_n + l + 1 < sizeof(got)) {
        memcpy(got + got_n, t, l);
        got_n += l;
        got[got_n] = 0;
    }

    got_calls++;
}

/* cw_decoder.c calls these only when no sink is set. The test always sets one,
 * but the symbols still have to resolve (the panel/remote server are not linked
 * into this binary). */
void pannel_add_text(const char *text) {
    (void) text;
}

void remote_text(const char *type, const char *s, size_t n) {
    (void) type;
    (void) s;
    (void) n;
}

void test_cw_decoder(void) {
    cw_decoder_set_sink(sink_text, NULL);

    /* Silence first: it lifts space_duration_prev above 0, so the first keyed
     * element does not take the degenerate dot/dash branch with prev == 0 (which
     * pins thr_mean at 0 and would hide the run). */
    for (int i = 0; i < 10; i++) {
        cw_decoder_signal(false, 500.0f);
    }

    /* The run: each key-down is under the 139 ms element threshold and there is
     * no inter-character space, so every key-up appends one element and no
     * character boundary is ever reached. 300 elements is more than two full
     * 128-element buffers. */
    for (int i = 0; i < 300; i++) {
        cw_decoder_signal(true, 20.0f);
        cw_decoder_signal(false, 20.0f);
    }

    /* Alive, and the run was sheared into decoded characters instead of
     * overrunning: at least two full buffers came out. */
    CHECK(got_calls >= 2);
    CHECK(strstr(got, "<?>") != NULL);

    /* A long silence flushes whatever was left of the run. */
    for (int i = 0; i < 5; i++) {
        cw_decoder_signal(false, 5000.0f);
    }

    /* The decoder is still usable: a single dot followed by a character space is
     * "E". */
    size_t before = got_n;
    cw_decoder_signal(true, 20.0f);
    cw_decoder_signal(false, 20.0f);
    cw_decoder_signal(false, 5000.0f);

    CHECK(got_calls > 2);
    CHECK(strchr(got + before, 'E') != NULL);
}

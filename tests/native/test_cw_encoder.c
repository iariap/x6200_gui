/*
 * Native test for src/cw_encoder.c (issue #116: "GUI crashea con SIGSEGV
 * (fallo=NULL) en el codigo de CW y deja la radio sin audio").
 *
 * The defect, measured on the radio (binary md5 3604cece0e43):
 *
 *   CRASHLOG sig=11 fallo=(nil) pc=0xb5de0064 lr=0xb5ddf714 sp=0xbeb89ae0 base=0x44ea74
 *
 * The stack resolved to libc: `pc` inside strlen, `lr` = the return address of
 * the `blx strlen` inside __strdup. The app frame right below is
 * `cw_encoder_send+0x68` (the `bl strdup` site, src/cw_encoder.c:167) and the
 * one above is `dialog_msg_cw_beacon_cb+0x50` (the `bl cw_encoder_send` site,
 * src/dialog_msg_cw.c:299). So the crash is `strdup(NULL)`: the CW messages
 * window called the keyer with a NULL message because its table was empty
 * (get_msg() returns NULL when table_rows == 0, and the msg_cw table ships
 * empty), and strdup() dereferences it in strlen().
 *
 * The invariant this test pins: cw_encoder_send() never dereferences a NULL
 * message -- a NULL text is a no-op that leaves the keyer idle and does not
 * touch the radio -- while a real message still keys. An empty message is a
 * no-op too: the keyer thread would advance past the terminator of the
 * strdup("") buffer (out-of-bounds read, found by this suite's ASan).
 *
 * The REAL src/cw_encoder.c is staged next to minimal stubs
 * (tests/native/stubs_cwenc/), the same pattern as the clock/backlight/
 * digital-modes/SWR-Scan tests.
 *
 * The stub runs endecode_thread() synchronously (tests/native/cwenc_stubs.c), so
 * the test process never has a second thread: the CI runner's GCC-13 ASan runtime
 * CHECKs inside AsanThread::Destroy() when a thread exits, and a thread-free test
 * cannot reach that path.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "test_framework.h"

#include "cw_encoder.h"

/* Provided by tests/native/cwenc_stubs.c. */
void cwenc_stub_reset(void);
int  cwenc_stub_key_toggles(void);
int  cwenc_stub_msg_updates(void);

void test_cw_encoder_null(void) {
    cwenc_stub_reset();

    /* The radio's crash: the CW messages window with an empty table. Before the
     * fix this line faults inside strlen (fallo=(nil)) and the process dies. */
    cw_encoder_send(NULL, false);
    CHECK(cw_encoder_state() == CW_ENCODER_IDLE);   /* nothing was armed */
    CHECK(cw_encoder_sent() == 0);
    CHECK(cwenc_stub_key_toggles() == 0);           /* the radio was not keyed */

    /* The beacon path (what the owner pressed) takes the same argument. */
    cw_encoder_send(NULL, true);
    CHECK(cw_encoder_state() == CW_ENCODER_IDLE);
    CHECK(cw_encoder_sent() == 0);
    CHECK(cwenc_stub_key_toggles() == 0);

    /* An empty (but non-NULL) message is also a no-op: endecode_thread advances past
     * the terminator of the strdup("") buffer and reads out of bounds (the ASan of
     * this suite found it), and get_msg() returns "" for an invalid cell. */
    cw_encoder_send("", false);
    CHECK(cw_encoder_state() == CW_ENCODER_IDLE);
    CHECK(cwenc_stub_key_toggles() == 0);
}

void test_cw_encoder_send(void) {
    cwenc_stub_reset();

    /* Positive control: a real message still arms the keyer and keys it (the NULL
     * guard did not turn cw_encoder_send() into a no-op). The thread stub runs
     * endecode_thread() synchronously, so by the time cw_encoder_send() returns the
     * message is already out: the radio was keyed (down + up) and the encoder is
     * back to IDLE. */
    cw_encoder_send("E", false);
    CHECK(cwenc_stub_key_toggles() == 2);            /* key down + key up */
    CHECK(cw_encoder_state() == CW_ENCODER_IDLE);
    CHECK(cw_encoder_sent() == 0);

    /* stop() over an idle encoder is a no-op. */
    cw_encoder_stop();
    CHECK(cw_encoder_state() == CW_ENCODER_IDLE);
}

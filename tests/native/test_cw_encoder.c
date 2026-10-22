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
int  cwenc_stub_voice_announcements(void);
void cwenc_stub_set_freq(int32_t freq);
void cwenc_stub_set_scope(int scope);

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

/*
 * CB TX interlock on the CW keyer (issue #160). The measured defect: with
 * cb_tx_scope = 0 (default, only the Argentine SBC 26.960-27.410 MHz), cw_send
 * at 26.515 MHz (grid B, outside the SBC) keyed the radio anyway. The keyer
 * closes the morse GPIO through radio_set_morse_key(), which never went through
 * radio_set_ptt() -- the only place that consulted cb_tx_allowed().
 *
 * The gate is now in the keyer: outside the window cw_encoder_send() refuses to
 * arm (no GPIO, message + voice, same as radio_set_ptt) and key_down() also
 * refuses per element, so a frequency change mid-message cannot keep keying.
 * The stub stages the REAL src/cb.c and moves cfg_cur.fg_freq /
 * params.cb_tx_scope, so this covers the whole chain, not a fake gate.
 *
 * cb_tx_allowed() itself is covered exhaustively by tests/native/test_cb.c; here
 * only the keyer's reaction to it is pinned.
 */
void test_cw_encoder_interlock(void) {
    /* --- cb_tx_scope = 0: only the SBC, 26.960-27.410 -------------------- */
    cwenc_stub_reset();
    cwenc_stub_set_scope(0);
    cwenc_stub_set_freq(26515000);              /* grid B: outside the SBC */
    cw_encoder_send("E", false);
    CHECK(cwenc_stub_key_toggles() == 0);       /* the GPIO was NOT closed */
    CHECK(cwenc_stub_msg_updates() == 1);       /* one "TX blocked" notice */
    CHECK(cwenc_stub_voice_announcements() == 1);
    CHECK(cw_encoder_state() == CW_ENCODER_IDLE);
    CHECK(cw_encoder_sent() == 0);

    /* The beacon path shares the same guard in cw_encoder_send(); it is not
     * exercised here because the test stub runs endecode_thread()
     * synchronously and the pre-fix beacon loops forever (a hang would mask
     * the failure). The guard is evaluated before the beacon branch, so a
     * blocked beacon is refused exactly like a normal send. */

    /* Inside the SBC the keyer is untouched. */
    cwenc_stub_reset();
    cwenc_stub_set_scope(0);
    cwenc_stub_set_freq(27185000);              /* CB C, CH 19 */
    cw_encoder_send("E", false);
    CHECK(cwenc_stub_key_toggles() == 2);       /* key down + key up */
    CHECK(cwenc_stub_msg_updates() == 0);
    CHECK(cwenc_stub_voice_announcements() == 0);

    /* 10 m (28.000 and up) is outside the interlock region: allowed. */
    cwenc_stub_reset();
    cwenc_stub_set_scope(0);
    cwenc_stub_set_freq(28074000);
    cw_encoder_send("E", false);
    CHECK(cwenc_stub_key_toggles() == 2);
    CHECK(cwenc_stub_msg_updates() == 0);

    /* A ham band far below the region: allowed too. */
    cwenc_stub_reset();
    cwenc_stub_set_scope(0);
    cwenc_stub_set_freq(14074000);
    cw_encoder_send("E", false);
    CHECK(cwenc_stub_key_toggles() == 2);

    /* --- cb_tx_scope = 1: the SBC plus grids A, B and D ------------------- */
    cwenc_stub_set_scope(1);
    const int32_t grids[3] = { 26065000, 26515000, 27415000 };   /* A, B, D */
    for (int i = 0; i < 3; i++) {
        cwenc_stub_reset();
        cwenc_stub_set_scope(1);
        cwenc_stub_set_freq(grids[i]);
        cw_encoder_send("E", false);
        CHECK(cwenc_stub_key_toggles() == 2);
        CHECK(cwenc_stub_msg_updates() == 0);
    }

    /* The gap between grids is not CB: still blocked even with scope 1. */
    cwenc_stub_reset();
    cwenc_stub_set_scope(1);
    cwenc_stub_set_freq(26510000);              /* hueco A-B */
    cw_encoder_send("E", false);
    CHECK(cwenc_stub_key_toggles() == 0);
    CHECK(cwenc_stub_msg_updates() == 1);

    /* --- cb_tx_scope = 2: no interlock at all ---------------------------- */
    cwenc_stub_reset();
    cwenc_stub_set_scope(2);
    cwenc_stub_set_freq(26515000);              /* would be blocked with 0/1 */
    cw_encoder_send("E", false);
    CHECK(cwenc_stub_key_toggles() == 2);
    CHECK(cwenc_stub_msg_updates() == 0);

    /* Leave the shared stub in its default state for any test that follows. */
    cwenc_stub_reset();
}

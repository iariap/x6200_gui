/*
 * Runner for the native CW keyboard idle-render test (tests/native/test_cw_kbd.c).
 * It is a separate binary because the real src/dialog_cw_kbd.c is compiled
 * against its own stub tree (tests/native/stubs_cwkbd/), not the shared stubs of
 * the main native suite.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"

void test_cw_kbd_idle_no_repaint(void);
void test_cw_kbd_reactive_state(void);
void test_cw_kbd_header_reacts(void);
void test_cw_kbd_theme_change_repaints(void);
void test_cw_kbd_reopen_resets_cache(void);
void test_cw_kbd_log_text_unchanged(void);
void test_cw_kbd_keyer_semantics(void);

int main(void) {
    test_cw_kbd_idle_no_repaint();
    test_cw_kbd_reactive_state();
    test_cw_kbd_header_reacts();
    test_cw_kbd_theme_change_repaints();
    test_cw_kbd_reopen_resets_cache();
    test_cw_kbd_log_text_unchanged();
    test_cw_kbd_keyer_semantics();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

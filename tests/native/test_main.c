/*
 * Runner for the native/ARM unit tests (see docs/TESTING.md).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "test_framework.h"




void test_adaptive_tuning(void);
void test_autotune(void);
void test_audio_enhancement(void);
void test_ft8_udp(void);
void test_update_check(void);
void test_band_plan(void);
void test_avi_writer(void);
void test_wfview(void);
void test_cluster(void);
void test_field(void);
void test_theme(void);
void test_controls(void);
void test_voice(void);
void test_gps_format(void);
void test_keyboard(void);
void test_kbd_caps(void);
void test_bt_devices(void);
void test_dxcc(void);
void test_flags(void);
void test_visual_policy(void);
void test_cw_decoder(void);

int main(void) {
    test_adaptive_tuning();
    test_autotune();
    test_audio_enhancement();
    test_ft8_udp();
    test_update_check();
    test_band_plan();
    test_avi_writer();
    test_wfview();
    test_cluster();
    test_field();
    test_theme();
    test_controls();
    test_voice();
    test_gps_format();
    test_keyboard();
    test_kbd_caps();
    test_bt_devices();
    test_dxcc();
    test_flags();
    test_visual_policy();
    test_cw_decoder();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

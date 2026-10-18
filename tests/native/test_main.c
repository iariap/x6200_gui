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
void test_cw_autotune(void);
void test_roe_curve(void);

/* SWR Scan multipasada (issue #125): el motor puro (src/swr_session.[ch]) y los fixtures del
 * owner. El dialogo tiene su propio binario (tests/native/test_swrscan.c). */
void test_swr_grid(void);
void test_swr_aggregation(void);
void test_swr_session_quick(void);
void test_swr_session_multipass(void);
void test_swr_session_alternate(void);
void test_swr_session_settle(void);
void test_swr_session_lifecycle(void);
void test_swr_session_shapes(void);
void test_swr_fixture_aggregates(void);
void test_swr_fixture_minimum(void);
void test_swr_fixture_delayed_flow(void);

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
    test_cw_autotune();
    test_roe_curve();

    test_swr_grid();
    test_swr_aggregation();
    test_swr_session_quick();
    test_swr_session_multipass();
    test_swr_session_alternate();
    test_swr_session_settle();
    test_swr_session_lifecycle();
    test_swr_session_shapes();
    test_swr_fixture_aggregates();
    test_swr_fixture_minimum();
    test_swr_fixture_delayed_flow();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

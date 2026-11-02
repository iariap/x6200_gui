/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Runner del stage A-prime (#367): coordinador de reg 12 + guardian fail-safe.
 */

#include <stdio.h>

#include "test_framework.h"

void test_rxmux_mask_preserves(void);
void test_rxmux_interleaved_1000(void);
void test_rxmux_coordinator_mock(void);
void test_rxmux_restore_mock(void);
void test_rxmux_ipc(void);
void test_rxmux_guardian_fsm(void);
void test_rxmux_process_recovery(void);
void test_rxmux_helper_lifecycle(void);

int main(void) {
    printf("== A-prime #367: dueno unico del reg 12 + guardian fail-safe ==\n");

    printf("  G1 mascara/invariantes (>=2009 valores)\n");
    test_rxmux_mask_preserves();
    printf("  G1 1000 eventos interleaved (idle/PTT/ATU/record/RX-TX)\n");
    test_rxmux_interleaved_1000();
    printf("  G1 coordinador con bus mock (interlock TX/ATU, readback mismatch)\n");
    test_rxmux_coordinator_mock();
    printf("  G1 restore selectivo preservando PTT/ATU (bus mock)\n");
    test_rxmux_restore_mock();
    printf("  G1 IPC/handshake (encode/decode, token/PID/starttime/readback)\n");
    test_rxmux_ipc();
    printf("  G1/G2 guardian FSM (lease 100 ms, heartbeat <=50 ms)\n");
    test_rxmux_guardian_fsm();
    printf("  G2 procesos reales: 100 repeticiones, restore medido\n");
    test_rxmux_process_recovery();
    printf("  G3 lifecycle del helper (INACTIVO, rechaza ARM sin readback)\n");
    test_rxmux_helper_lifecycle();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}

/* Minimal dialog_swrscan.h stub for the radio_thread test (issue #219): the two
 * entry points radio_tick() uses (update per packet, poll per watchdog tick).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

/* Potencia del barrido, en decimas de watt (copia de src/dialog_swrscan.h: el
 * control de potencia del panel tiene piso 0.1 W). */
#define SWRSCAN_TX_POWER_DW     1
#define SWRSCAN_PWR_FLOOR_DW    1
#define SWRSCAN_PWR_CEILING_DW  80
#define SWRSCAN_TX_POWER_W      ((float) SWRSCAN_TX_POWER_DW / 10.0f)

void dialog_swrscan_update(float vswr);
void dialog_swrscan_poll(void);

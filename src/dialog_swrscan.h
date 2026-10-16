/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdint.h>

#include "lvgl/lvgl.h"
#include "dialog.h"

extern dialog_t *dialog_swrscan;

/* Barrido de ROE automatico (issue #113; ampliaciones del owner del 2026-10-16: "aumentar la
 * frecuencia de barrido como lo hace el nanovna" y "la potencia de transmision durante el
 * barrido debe ser la minima posible").
 *
 * El conteo de puntos NO es fijo: sale del span que se esta mirando (como el NanoVNA, denso en
 * spans chicos y acotado en los anchos, dialog_swrscan_points_for_span). El barrido TRANSMITE
 * mientras mide y avanza un punto por paquete del flow de la radio (do_step se llama desde
 * RADIO_SWRSCAN): un paquete tarda ~35 ms (aether_radio/.../low/flow.h: "Usually a packet
 * arrives every 35ms"), asi que la duracion del barrido es puntos x dwell. El dwell NO es un
 * parametro de la GUI: aether_control solo expone x6200_control_swrscan_set(on/off), sin
 * cadencia.
 *
 * Con la potencia en el piso del equipo el PA dejo de ser la restriccion (ver
 * SWRSCAN_TX_POWER_DW: 0.1 W son 14.8 dB menos que los 3 W que ponia el barrido), asi que lo que
 * acota es la espera del operador. SWRSCAN_TX_BUDGET_MS es explicito y subirlo es decision del
 * owner, no del codigo: el _Static_assert de dialog_swrscan.c hace fallar el build si el barrido
 * se pasa del presupuesto. */
#define SWRSCAN_STEP_TARGET_HZ  1250  /* paso al que apunta el muestreo (Hz); nunca mas fino */
#define SWRSCAN_MIN_POINTS      41    /* piso del conteo: los spans chicos quedan densos */
#define SWRSCAN_MAX_POINTS      51    /* techo del conteo: los spans anchos quedan acotados */
#define SWRSCAN_DWELL_MS        35    /* lo que tarda un paquete del flow de la radio */
#define SWRSCAN_TX_BUDGET_MS    1785  /* duracion maxima del barrido: SWRSCAN_MAX_POINTS x dwell */

/* Potencia del barrido, en decimas de watt (la unidad del registro: aether_control hace
 * p = pwr * 10 y lo escribe en el byte bajo de rfg_txpwr).
 *
 * El piso del equipo es 0.1 W y no es una suposicion: es el minimo que acepta el control de
 * potencia del panel (src/vol.c, VOL_PWR: LV_MIN(8.0f, ...) / LV_MAX(0.1f, ...)) y el del control
 * remoto (src/remote_server.cpp: {"pwr", CMD_PWR, 1, 100} = decimas de watt). El barrido venia
 * transmitiendo 3 W fijos (radio_start_swrscan): 0.1 W son 14.8 dB menos.
 *
 * El NanoVNA mide a ~0 dBm (1 mW) y el X6200 no llega ahi: 0.1 W = 20 dBm, 20 dB por encima del
 * NanoVNA, pero es el piso del equipo (mas abajo se sale del rango que declara su control). */
#define SWRSCAN_TX_POWER_DW     1     /* decimas de watt: el piso del equipo (0.1 W) */
#define SWRSCAN_PWR_FLOOR_DW    1     /* piso del control del equipo: 0.1 W (src/vol.c) */
#define SWRSCAN_PWR_CEILING_DW  80    /* techo del control del equipo: 8.0 W (src/vol.c) */
#define SWRSCAN_TX_POWER_W      ((float) SWRSCAN_TX_POWER_DW / 10.0f)

/* Puntos del barrido para `span_hz` (modo automatico): span / SWRSCAN_STEP_TARGET_HZ acotado a
 * [SWRSCAN_MIN_POINTS, SWRSCAN_MAX_POINTS] y ajustado a un divisor exacto del span
 * (span % (puntos - 1) == 0, ver la geometria de abajo). Con los 4 spans del dialogo:
 * 50 kHz -> 41 (paso 1250 Hz); 100 / 200 / 500 kHz -> 51 (2000 / 4000 / 10000 Hz). */
int dialog_swrscan_points_for_span(int32_t span_hz);

/* Frecuencia (Hz) del punto `index` (0 .. puntos-1) del barrido. La geometria es borde a borde:
 * el span [center - span/2, center + span/2] se parte en (puntos - 1) intervalos exactos
 * (span / (puntos - 1), sin resto) y el punto k cae en center - span/2 + k * paso, asi que
 * index 0 es el borde bajo y index puntos-1 el borde alto del span. */
uint32_t dialog_swrscan_sweep_freq(uint32_t center, int32_t span_hz, int index);

void dialog_swrscan_update(float vswr);

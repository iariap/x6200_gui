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

/* Configuracion del barrido de ROE (issue #113; ampliacion del owner: "aumentar la
 * frecuencia de barrido como lo hace el nanovna").
 *
 * El barrido TRANSMITE mientras mide (radio_start_swrscan pone 3 W en LSB) y avanza un
 * punto por paquete del flow de la radio (do_step se llama desde RADIO_SWRSCAN). Un
 * paquete del flow tarda ~35 ms (aether_radio/.../low/flow.h: "Usually a packet arrives
 * every 35ms"), asi que el tiempo al aire del barrido es puntos x dwell. El dwell NO es
 * un parametro de la GUI: aether_control solo expone x6200_control_swrscan_set(on/off),
 * sin cadencia. Por eso mas puntos = mas tiempo de TX, y el techo es
 * SWRSCAN_TX_BUDGET_MS: subirlo es decision del owner, no del codigo (el _Static_assert
 * de dialog_swrscan.c hace fallar el build si el barrido se pasa del techo). */
#define SWRSCAN_POINTS        50    /* muestras por barrido, una por paquete del flow */
#define SWRSCAN_DWELL_MS      35    /* lo que tarda un paquete del flow de la radio */
#define SWRSCAN_TX_BUDGET_MS  1750  /* techo de TX del barrido: 50 x 35 ms, lo que dura hoy */

/* Frecuencia (Hz) del punto `index` del barrido: el span [center - span/2, center + span/2]
 * se parte en SWRSCAN_POINTS pasos exactos (span / SWRSCAN_POINTS) y el punto k cae en
 * center - span/2 + k * paso. Las muestras del barrido son index 0 .. SWRSCAN_POINTS-1
 * (index 0 es el borde bajo del span); index SWRSCAN_POINTS devuelve el borde alto, asi el
 * barrido va de punta a punta del span (los extremos en los limites). */
uint32_t dialog_swrscan_sweep_freq(uint32_t center, int32_t span_hz, int index);

void dialog_swrscan_update(float vswr);

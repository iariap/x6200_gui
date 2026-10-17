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

/* Barrido de ROE automatico (issues #113 y #125).
 *
 * #125 (este issue) reemplaza el barrido de "un paquete = un punto" y el filtro movil de 5
 * puntos por una SESION de medicion multipasada (src/swr_session.[ch]):
 *  - 51 puntos por barrido, independientes del span (los 4 spans del dialogo solo cambian el
 *    paso: 1 / 2 / 4 / 10 kHz), construidos una sola vez por sesion con aritmetica de 64 bits;
 *  - varias pasadas sobre EXACTAMENTE la misma grilla y agregacion por frecuencia (mediana),
 *    nunca entre frecuencias vecinas;
 *  - la lectura valida no se atribuye a ciegas a la frecuencia recien pedida (ventana de
 *    asentamiento) y una lectura faltante NUNCA se convierte en ROE 1.0;
 *  - el minimo sale del dato medido agregado, no del vertice del ajuste que se dibuja.
 *
 * El barrido TRANSMITE mientras mide: el presupuesto SWRSCAN_TX_BUDGET_MS es el de UNA pasada
 * y NO se sube automaticamente. El dwell no es un parametro de la GUI (aether_control solo
 * expone x6200_control_swrscan_set(on/off)); un paquete del flow tarda ~35 ms. */
#define SWRSCAN_DWELL_MS        35    /* lo que tarda un paquete del flow de la radio */
#define SWRSCAN_TX_BUDGET_MS    1785  /* duracion maxima de UNA pasada: 51 x dwell */

/* Watchdog de estancamiento (no es el presupuesto de TX): si el flow deja de mandar paquetes,
 * la sesion se aborta y el TX se apaga. Se mide desde el inicio de la pasada/sesion y tiene
 * holgura sobre la duracion esperada para no cortar una pasada normal. */
#define SWRSCAN_PASS_TIMEOUT_MS (SWRSCAN_TX_BUDGET_MS + 500)

/* Ventana de asentamiento: tiempo, desde que se ordena la frecuencia, durante el cual un
 * paquete todavia puede traer la lectura de la frecuencia anterior (stale) y se descarta. El
 * flow NO confirma cuando el VFO se movio, asi que 0 significa "atribuir el primer paquete a la
 * frecuencia pedida" (lo que hacia #113, ahora explicito). Subirlo exige medir el dwell real de
 * la radio en hardware (carga ficticia): decision del owner. El motor lo implementa y lo prueba
 * con el flow de fixture (tests/native/test_swrscan_flow.c); el dialogo corre con 0. */
#define SWRSCAN_SETTLE_MS       0

/* Multipasada (3 = Stable, 5 = Fine) sobre TX real: DESHABILITADA.
 *
 * El presupuesto por pasada (SWRSCAN_TX_BUDGET_MS) esta validado para UNA pasada; 3/5 pasadas
 * triplican/quintuplican el tiempo de TX de la sesion (5355 / 8925 ms) y exigen validacion en
 * hardware (carga ficticia 50 ohm, potencia entregada y precision del detector a 0.1 W,
 * temperatura/consumo) y decision explicita del owner. El MOTOR (swr_session) SI soporta N
 * pasadas: lo prueban los tests con el backend fake; lo que esta apagado es habilitar ese TX
 * desde el dialogo. Cambiar a 1 solo con orden del owner y evidencia de banco.
 *
 * El valor se puede pisar SOLO en tiempo de compilacion (build de test) para que la suite pueda
 * ejercitar el camino de 3/5 pasadas de punta a punta con el backend fake y demostrar que el
 * gate funciona: el binario que se flashea se compila sin -D y queda en 0. */
#ifndef SWRSCAN_MULTIPASS_TX_ENABLED
#define SWRSCAN_MULTIPASS_TX_ENABLED 0
#endif

/* Potencia del barrido, en decimas de watt (la unidad del registro: aether_control hace
 * p = pwr * 10 y lo escribe en el byte bajo de rfg_txpwr).
 *
 * El piso del equipo es 0.1 W y no es una suposicion: es el minimo que acepta el control de
 * potencia del panel (src/vol.c, VOL_PWR: LV_MAX(0.1f, ...)) y el del control remoto
 * (src/remote_server.cpp: {"pwr", CMD_PWR, 1, 100} = decimas de watt). */
#define SWRSCAN_TX_POWER_DW     1     /* decimas de watt: el piso del equipo (0.1 W) */
#define SWRSCAN_PWR_FLOOR_DW    1     /* piso del control del equipo: 0.1 W (src/vol.c) */
#define SWRSCAN_PWR_CEILING_DW  80    /* techo del control del equipo: 8.0 W (src/vol.c) */
#define SWRSCAN_TX_POWER_W      ((float) SWRSCAN_TX_POWER_DW / 10.0f)

/* Estado visible del dialogo, para el readout (issue #125). */
const char *dialog_swrscan_state_text(void);

/* Un paquete del flow de la radio (RADIO_SWRSCAN) mientras el barrido corre. Corre en el hilo
 * de la radio: NO toca LVGL, solo encola un refresh y llama a radio_set_freq(). */
void dialog_swrscan_update(float vswr);

/* Watchdog del barrido cuando el flow se queda sin paquetes (hilo de la radio). */
void dialog_swrscan_poll(void);

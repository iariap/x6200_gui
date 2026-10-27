/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stddef.h>
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
 * expone x6200_control_swrscan_set(on/off)).
   *
   * MEDIDO EN LA RADIO (2026-10-27, 17m y 40m, 51 puntos, binario instrumentado): deltas
   * entre muestras validas media 48,4 ms, maximo 63 ms; el primer punto tarda ~513 ms
   * desde Run. El valor anterior (35 ms) era una suposicion del comentario de flow.h
   * ("Usually a packet arrives every 35ms") y hacia que la pasada abortara por RF
   * timeout a los 33-43 de 51 puntos. Se usa el peor caso medido + margen (70 ms):
   * 51 x 70 = 3570 ms de techo de TX por pasada (el duenio aprobo subir el techo el
   * 27/10 para medir la ROE completa; sigue a 0,1 W y una sola pasada). */
#define SWRSCAN_DWELL_MS        70    /* peor caso medido en la radio + margen */
#define SWRSCAN_TX_BUDGET_MS    3570  /* duracion maxima de UNA pasada: 51 x dwell */

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
 * triplican/quintuplican el tiempo de TX de la sesion (10710 / 17850 ms) y exigen validacion en
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

/* Control remoto del barrido (issue #133): `{"cmd":"swrscan","val":1|0}`.
 *
 * `start` = true arranca la MISMA sesion que el boton Run y false la detiene por el MISMO
 * camino que el boton Stop: esta funcion solo llama a dialog_swrscan_run_cb(), no duplica
 * la logica de potencia/modo/restore (radio_start_swrscan/radio_stop_swrscan, #94/#102) ni
 * la de la sesion (swr_session, #125).
 *
 * Guardas duras (el barrido TRANSMITE, a 0.1 W):
 *  - solo actua con la ventana del SWR Scan abierta (dialog_type_is_run); si la app se cerro
 *    entre el pedido y la ejecucion en el hilo de LVGL, devuelve false sin tocar la radio;
 *  - arrancar exige el equipo en RADIO_RX (nunca inicia TX sobre una TX ajena);
 *  - idempotente: Run con el barrido corriendo (o Stop con el barrido quieto) es un no-op y
 *    devuelve true; un segundo Run NO reinicia la sesion ni vuelve a transmitir.
 *
 * Devuelve false cuando no actuo (app cerrada, equipo ocupado o el gate del dialogo rechazo
 * el arranque: multipasada, rango, parametros). El estado se consulta por `get` (abajo).
 * Solo se llama desde el hilo de LVGL (el scheduler de src/remote_server.cpp). */
bool dialog_swrscan_remote_run(bool start);

/* Adaptador para app_cmds[] de src/remote_server.cpp (firma comun de las apps): `cmd` es
 * "swrscan" y `val` 1 = Run, 0 = Stop. El texto no se usa. */
void dialog_swrscan_remote(const char *cmd, int32_t val, const char *text);

/* Estado del barrido para el campo `ax` de `{"cmd":"get"}` (issue #133):
 *   {"swrscan_state":"Idle|Scanning|Completed|Stopped|Error","passes":N,"pass":N,
 *    "points":N,"swrscan_tx":true|false}
 * Seguro desde el hilo del socket: solo lee escalares, no toca LVGL. */
int dialog_swrscan_remote_state(char *buf, size_t size);

/* Un paquete del flow de la radio (RADIO_SWRSCAN) mientras el barrido corre. Corre en el hilo
 * de la radio: NO toca LVGL, solo encola un refresh y llama a radio_set_freq(). */
void dialog_swrscan_update(float vswr);

/* Watchdog del barrido cuando el flow se queda sin paquetes (hilo de la radio). */
void dialog_swrscan_poll(void);

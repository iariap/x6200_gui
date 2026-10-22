/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Estado del Bluetooth, para el icono de la barra y para Ajustes (ver bt_state.c).
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bluetooth prendido: es el BTCAT_BT que escriben btcat-ctl y los scripts (mismos archivos:
 * /etc/default/btcat y /mnt/btcat.conf). */
bool bt_state_on(void);

/* "Discoverable (CAT)": BTCAT_DISCOVERABLE, el mismo que usa el puente CAT. */
bool bt_state_discoverable(void);

/* Hay auriculares A2DP conectados: btaudio-ctl escribe /tmp/btaudio-hp con los sinks cuando los
 * hay y lo borra cuando no (ver /usr/sbin/btaudio-ctl). */
bool bt_state_connected(void);

/* BTAUDIO_MUTE del script: mientras hay auriculares conectados, la GUI tiene que dejar el audio
 * del equipo en 0 para que no suene tambien el altavoz. */
bool bt_state_mute(void);

#ifdef __cplusplus
}
#endif

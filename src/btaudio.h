/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Hablar con btaudio-ctl (los auriculares A2DP) desde la GUI, ver btaudio.c.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Deja una linea en /mnt/app_logs/bt.log: es la unica forma de ver que hizo el Bluetooth (el
 * script escribe en /dev/null y sus resultados se pierden). */
void btaudio_log(const char *fmt, ...);

/* Corre "btaudio-ctl <args>" en un hilo aparte: el script tarda (bluetoothctl, pactl) y no se
 * puede esperar desde la GUI. */
void btaudio_run(const char *fmt, ...);

/* Volumen de los auriculares (btaudio-ctl volume N). Solo hace algo si hay auriculares
 * conectados: es el nivel del sink A2DP, no el del equipo. */
void btaudio_headphone_volume(int pct);

/* Honra BTAUDIO_MUTE: mientras hay auriculares conectados el script espera que la GUI deje el
 * audio del equipo en 0 (el loopback manda la recepcion a los auriculares y el altavoz seguiria
 * sonando). Usa el mute que ya tiene el equipo (radio_change_mute). */
void btaudio_speaker_mute(bool on);

/* Se llama desde el timer que ya mira el estado de BT (info.c): cuando los auriculares se
 * conectan y BTAUDIO_MUTE esta en yes, deja el audio del equipo en 0; al desconectarse lo
 * devuelve, pero solo si el mute lo habia puesto esto. */
void btaudio_follow_state(void);

#ifdef __cplusplus
}
#endif

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Deteccion de teclado por sysfs.
 *
 *  En este equipo abrir un /dev/input/eventN cuesta ~50 ms: las perillas y el teclado del panel
 *  cuelgan del bus RSB y su driver hace una transaccion por apertura. Buscar un teclado USB
 *  probando EVIOCGBIT en cada event* se llevaba 0,60 s del arranque, asi que el mapa de teclas
 *  se mira en /sys/class/input/eventN/device/capabilities/key, que el kernel ya publica.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Interpreta una linea de capabilities/key. Devuelve 1 si el dispositivo tiene la fila de
 * letras Q..P y la barra espaciadora, 0 si no, y -1 si la linea no se entiende.
 */
int kbd_caps_letras(const char *linea);

/*
 * Lo mismo, leyendo sysfs para /dev/input/<evento>. Devuelve -1 si sysfs no esta (el que llama
 * tiene que abrir el dispositivo y preguntar por EVIOCGBIT).
 */
int kbd_caps_letras_de_dev(const char *dev_path);

#ifdef __cplusplus
}
#endif

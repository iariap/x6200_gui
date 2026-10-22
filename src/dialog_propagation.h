/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - App "Propagation" (issue #145)
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_propagation;

/* Control remoto (issue #145): `{"cmd":"propagation","val":1|0}`. 1 = Run,
 * 0 = Cancelar; es idempotente y solo actua con la ventana abierta. Corre en el
 * hilo de LVGL (el scheduler del servidor remoto). */
bool dialog_propagation_remote_run(bool start);
void dialog_propagation_remote(const char *cmd, int32_t val, const char *text);

/* Estado para el campo `ax` de `{"cmd":"get"}`. Seguro desde el hilo del socket
 * (solo lee escalares bajo el lock del dialogo, sin tocar LVGL). */
int dialog_propagation_remote_state(char *buf, size_t size);

#ifdef __cplusplus
}
#endif

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - App "Log uploads": credenciales de los servicios de log
 *  (QRZ, eQSL, Clublog) editables desde la UI. Escribe /mnt/upload.conf, el mismo
 *  archivo que lee el daemon /usr/share/x6200/qrz-upload.py.
 */
#pragma once

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_logs;

#ifdef __cplusplus
}
#endif

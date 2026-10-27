/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - configuracion de la subida de QSO a logs online.
 *
 *  Lee y escribe /mnt/upload.conf, el mismo archivo que consume el daemon
 *  /usr/share/x6200/qrz-upload.py. Sin LVGL: se puede compilar y probar en la PC.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool enabled;
    int  interval;

    bool qrz_enabled;
    char qrz_api_key[96];

    bool eqsl_enabled;
    char eqsl_user[64];
    char eqsl_pswd[96];

    bool clublog_enabled;
    char clublog_callsign[24];
    char clublog_email[96];
    char clublog_password[96];
    char clublog_api[96];
} upload_conf_t;

/* Ruta del archivo: UPLOAD_CONF si esta definida (tests), si no /mnt/upload.conf. */
const char *upload_conf_path(void);

/* Carga el archivo; si no existe deja los valores por defecto y devuelve false. */
bool upload_conf_load(upload_conf_t *conf);

/* Escribe el archivo completo (atomo: .tmp + rename, permisos 0600). */
bool upload_conf_save(const upload_conf_t *conf);

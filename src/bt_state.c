/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Estado del Bluetooth, compartido entre la barra de estado (info.c) y Ajustes
 *  (dialog_settings.c): antes cada uno leia los archivos por su cuenta.
 */

#include "bt_state.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Los mismos dos archivos que lee btcat-ctl: el de fabrica y el que se cambia desde Ajustes. */
static bool lee_clave(const char *key, bool def) {
    static const char *const files[] = { "/etc/default/btcat", "/mnt/btcat.conf" };
    size_t klen = strlen(key);
    bool   val  = def;

    for (unsigned i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        FILE *fp = fopen(files[i], "r");

        if (fp == NULL) {
            continue;
        }

        char line[128];

        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                const char *v = line + klen + 1;

                if (*v == '"') {
                    v++;
                }

                val = strncmp(v, "yes", 3) == 0;
            }
        }

        fclose(fp);
    }

    return val;
}

bool bt_state_on(void) {
    return lee_clave("BTCAT_BT", true);
}

bool bt_state_discoverable(void) {
    return lee_clave("BTCAT_DISCOVERABLE", true);
}

bool bt_state_connected(void) {
    return access("/tmp/btaudio-hp", F_OK) == 0;
}

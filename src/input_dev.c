/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - busqueda de dispositivos de entrada por nombre (ver input_dev.h).
 */

#include "input_dev.h"

#include "lvgl/lvgl.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEV_DIR     "/dev/input"
#define NAME_MAX_DE 64

const char * input_dev_by_name(const char *name, const char *fallback) {
    static char ruta[64];
    DIR        *d;
    struct dirent *e;

    if (name == NULL) {
        return fallback;
    }

    d = opendir(DEV_DIR);

    if (d == NULL) {
        return fallback;
    }

    while ((e = readdir(d)) != NULL) {
        char  nombre[NAME_MAX_DE];
        int   fd;
        char  ruta_tmp[64];

        if (strncmp(e->d_name, "event", 5) != 0) {
            continue;
        }

        snprintf(ruta_tmp, sizeof(ruta_tmp), DEV_DIR "/%s", e->d_name);
        fd = open(ruta_tmp, O_RDONLY | O_NONBLOCK);

        if (fd < 0) {
            continue;
        }

        memset(nombre, 0, sizeof(nombre));

        if (ioctl(fd, EVIOCGNAME(sizeof(nombre) - 1), nombre) >= 0 && strcmp(nombre, name) == 0) {
            close(fd);
            closedir(d);
            snprintf(ruta, sizeof(ruta), "%s", ruta_tmp);
            LV_LOG_USER("Entrada '%s' encontrada en %s", name, ruta);
            return ruta;
        }

        close(fd);
    }

    closedir(d);
    LV_LOG_WARN("No encontre '%s' entre los /dev/input/event*: uso %s", name,
                fallback ? fallback : "(nada)");
    return fallback;
}

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

/*
 * El nombre del dispositivo tambien esta en sysfs (/sys/class/input/eventN/device/name), y
 * leerlo no cuesta nada. Abrir el /dev/input/eventN para preguntarselo, en cambio, cuesta
 * unos 50 ms en este equipo: las perillas y el teclado interno estan en el bus RSB y su
 * driver hace una transaccion por apertura. Con cuatro perillas y varios event* el sondeo
 * se llevaba ~1,5 s del arranque, asi que primero se pregunta por sysfs y solo si eso no
 * esta disponible (kernel sin sysfs) se abre el dispositivo.
 */
static bool nombre_por_sysfs(const char *evento, char *nombre, size_t size) {
    char  ruta[128];
    FILE *f;
    size_t n;

    snprintf(ruta, sizeof(ruta), "/sys/class/input/%s/device/name", evento);
    f = fopen(ruta, "r");

    if (f == NULL) {
        return false;
    }

    if (fgets(nombre, size, f) == NULL) {
        fclose(f);
        return false;
    }

    fclose(f);

    n = strlen(nombre);

    while (n > 0 && (nombre[n - 1] == '\n' || nombre[n - 1] == '\r')) {
        nombre[--n] = '\0';
    }

    return n > 0;
}

char * input_dev_by_name(const char *name, const char *fallback) {
    static char ruta[64];
    DIR        *d;
    struct dirent *e;

    if (name == NULL) {
        /* los llamadores sólo leen la ruta (open), no la escriben */
        return (char *) fallback;
    }

    d = opendir(DEV_DIR);

    if (d == NULL) {
        return (char *) fallback;
    }

    while ((e = readdir(d)) != NULL) {
        char  nombre[NAME_MAX_DE];
        int   fd;
        char  ruta_tmp[64];

        if (strncmp(e->d_name, "event", 5) != 0) {
            continue;
        }

        snprintf(ruta_tmp, sizeof(ruta_tmp), DEV_DIR "/%s", e->d_name);

        if (nombre_por_sysfs(e->d_name, nombre, sizeof(nombre))) {
            if (strcmp(nombre, name) == 0) {
                closedir(d);
                snprintf(ruta, sizeof(ruta), "%s", ruta_tmp);
                LV_LOG_USER("Entrada '%s' encontrada en %s (por sysfs)", name, ruta);
                return ruta;
            }

            continue;   /* ya sabemos como se llama: no hace falta abrirlo */
        }

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
    return (char *) fallback;
}

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Saber si un dispositivo de entrada tiene letras SIN abrirlo (ver kbd_caps.h).
 */

#include "kbd_caps.h"

#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * El mapa de teclas del kernel viene en hexadecimal, con las palabras de 64 bits de la mas
 * significativa a la menos significativa, separadas por espacios: la ULTIMA palabra son los
 * bits 0..63 (ejemplo real de un teclado de PC:
 * "402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe").
 *
 * Se pide lo mismo que preguntaba has_letters() con EVIOCGBIT: la fila de letras Q..P y la
 * barra espaciadora.
 */
int kbd_caps_letras(const char *linea) {
    const char        *ultimo;
    char              *fin;
    unsigned long long bits;
    int                k;

    if (linea == NULL) {
        return -1;
    }

    ultimo = strrchr(linea, ' ');
    ultimo = (ultimo != NULL) ? ultimo + 1 : linea;

    bits = strtoull(ultimo, &fin, 16);

    if (fin == ultimo) {   /* no habia ningun digito hexadecimal */
        return -1;
    }

    for (k = KEY_Q; k <= KEY_P; k++) {
        if ((bits & (1ULL << k)) == 0) {
            return 0;
        }
    }

    return (bits & (1ULL << KEY_SPACE)) != 0;
}

int kbd_caps_letras_de_dev(const char *dev_path) {
    const char *evento;
    char        ruta[160];
    char        linea[512];
    FILE       *f;
    int         r;

    if (dev_path == NULL) {
        return -1;
    }

    evento = strrchr(dev_path, '/');
    evento = (evento != NULL) ? evento + 1 : dev_path;

    snprintf(ruta, sizeof(ruta), "/sys/class/input/%s/device/capabilities/key", evento);
    f = fopen(ruta, "r");

    if (f == NULL) {
        return -1;   /* sin sysfs: que decida el que llama (abriendo el dispositivo) */
    }

    if (fgets(linea, sizeof(linea), f) == NULL) {
        fclose(f);
        return -1;
    }

    fclose(f);

    r = kbd_caps_letras(linea);

    return (r < 0) ? -1 : r;
}

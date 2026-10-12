/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - formateo de coordenadas (ver gps_format.h).
 */

#include "gps_format.h"

#include <math.h>
#include <stdio.h>

void gps_format_coord(double grados, const char *sufijo_pos, const char *sufijo_neg, char *buf, size_t n) {
    const char *sufijo = "";
    double      f, minutos;
    int         entero, min;

    if (buf == NULL || n == 0) {
        return;
    }

    if (!isfinite(grados) || fabs(grados) > 180.0) {
        snprintf(buf, n, "N/A");
        return;
    }

    if (grados < 0.0) {
        grados = -grados;

        if (sufijo_neg != NULL) {
            sufijo = sufijo_neg;
        }
    } else if (sufijo_pos != NULL) {
        sufijo = sufijo_pos;
    }

    minutos = modf(grados, &f) * 60.0;
    entero  = (int) f;
    min     = (int) minutos;

    /* cuatro decimales de minuto = ~2 m, de sobra */
    snprintf(buf, n, "%i %02i.%04i'%s", entero, min, (int) ((minutos - min) * 10000.0 + 0.5), sufijo);
}

void gps_format_age(int segundos, char *buf, size_t n) {
    if (buf == NULL || n == 0) {
        return;
    }

    if (segundos < 0) {
        snprintf(buf, n, "sin datos");
    } else if (segundos < 60) {
        snprintf(buf, n, "%i s", segundos);
    } else if (segundos < 3600) {
        snprintf(buf, n, "%i min", segundos / 60);
    } else {
        snprintf(buf, n, "%i h %i min", segundos / 3600, (segundos % 3600) / 60);
    }
}

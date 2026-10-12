/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - formateo de coordenadas para la app de GPS.
 *
 *  Sin dependencias: lo cubren los tests nativos.
 */

#pragma once

#include <stddef.h>

/* Grados y minutos con cuatro decimales y el sufijo del hemisferio, que es como se leen las
 * coordenadas en radio: 45.123456 N -> "45 07.4074'N". Un valor fuera de rango o no finito da
 * "N/A". */
void gps_format_coord(double grados, const char *sufijo_pos, const char *sufijo_neg, char *buf, size_t n);

/* "hace 12 s" / "hace 3 min" / "hace 1 h 5 min" */
void gps_format_age(int segundos, char *buf, size_t n);

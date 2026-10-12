/*
 * Unit tests: el formateo de coordenadas de la app de GPS (gps_format.c).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <string.h>

#include "gps_format.h"
#include "test_framework.h"

void test_gps_format(void) {
    char buf[64];

    /* un valor normal: grados, minutos con decimales y el hemisferio */
    gps_format_coord(45.123456, "N", "S", buf, sizeof(buf));
    CHECK(strncmp(buf, "45 07.", 6) == 0);
    CHECK(strchr(buf, 'N') != NULL);

    /* negativo: usa el sufijo negativo */
    gps_format_coord(-7.7654, "N", "S", buf, sizeof(buf));
    CHECK(strncmp(buf, "7 45.", 5) == 0);
    CHECK(strchr(buf, 'S') != NULL);

    /* fuera de rango o sin datos */
    gps_format_coord(200.0, "E", "W", buf, sizeof(buf));
    CHECK(strcmp(buf, "N/A") == 0);

    gps_format_coord(NAN, "E", "W", buf, sizeof(buf));
    CHECK(strcmp(buf, "N/A") == 0);

    /* la antigüedad se cuenta como uno la lee */
    gps_format_age(12, buf, sizeof(buf));
    CHECK(strcmp(buf, "12 s") == 0);

    gps_format_age(120, buf, sizeof(buf));
    CHECK(strcmp(buf, "2 min") == 0);

    gps_format_age(3700, buf, sizeof(buf));
    CHECK(strcmp(buf, "1 h 1 min") == 0);

    gps_format_age(-1, buf, sizeof(buf));
    CHECK(strcmp(buf, "sin datos") == 0);
}

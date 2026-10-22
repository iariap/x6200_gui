/*
 * Unit tests: los topes y pasos de los controles de las apps (controls_step.c).
 *
 * Cuando una app toma un control, los giros del MFK mueven ESE control y no el parámetro del
 * MFK de la pantalla principal. Acá se cubren el paso y los topes de los dos controles que
 * necesita este port (WeFax align y tilt: -1300..1300 y -400..400, como en el X6100) y que un
 * control desconocido no cambie nada.
 *
 * controls_step.c no tiene dependencias a propósito, así que se prueba en el host.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "controls_ids.h"
#include "controls_step.h"
#include "test_framework.h"

void test_controls(void) {
    /* un giro mueve el control en su paso */
    CHECK(controls_step(CTRL_WEFAX_ALIGN, 0, 8) == 8);
    CHECK(controls_step(CTRL_WEFAX_ALIGN, 8, -8) == 0);
    CHECK(controls_step(CTRL_WEFAX_ALIGN, 100, 8) == 108);

    CHECK(controls_step(CTRL_WEFAX_TILT, 0, 1) == 1);
    CHECK(controls_step(CTRL_WEFAX_TILT, 1, -2) == -1);
    CHECK(controls_step(CTRL_WEFAX_TILT, 10, 1) == 11);

    /* los topes del X6100 */
    CHECK(controls_step(CTRL_WEFAX_ALIGN, 1300, 8) == 1300);
    CHECK(controls_step(CTRL_WEFAX_ALIGN, -1300, -8) == -1300);

    CHECK(controls_step(CTRL_WEFAX_TILT, 400, 1) == 400);
    CHECK(controls_step(CTRL_WEFAX_TILT, -400, -2) == -400);

    /* un control que no es de este port no toca nada */
    CHECK(controls_step(9999, 123, 8) == 123);
    CHECK(controls_step(0, -5, 8) == -5);
}

/*
 * Unit tests: deteccion de teclado por sysfs (kbd_caps.c).
 *
 * En este equipo abrir un /dev/input/eventN cuesta ~50 ms (bus RSB), asi que el arranque no
 * puede probar cada event* con EVIOCGBIT para saber si es un teclado. La decision se toma
 * leyendo el mapa de teclas que el kernel publica en sysfs, y ese parseo es logica pura:
 * se prueba aca con lineas reales.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>

#include "kbd_caps.h"
#include "test_framework.h"

void test_kbd_caps(void) {
    /* Teclado AT real de una PC (event3 del banco de desarrollo): fila Q..P y espacio */
    CHECK(kbd_caps_letras("402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe\n") == 1);
    /* Sin salto de linea final */
    CHECK(kbd_caps_letras("402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe") == 1);

    /* Boton de encendido: "8000 10000000000000 0" (ultima palabra = bits 0..63 = 0) */
    CHECK(kbd_caps_letras("8000 10000000000000 0\n") == 0);
    /* Un dispositivo sin ninguna capacidad */
    CHECK(kbd_caps_letras("0\n") == 0);
    /* Rueda/encoder interno: solo tiene bits de rotacion (mas alla del bit 63) */
    CHECK(kbd_caps_letras("400000000010040 40000 18040000 f000000000000000\n") == 0);

    /* Tiene Q..P (bits 16..25 = 0x3ff0000) pero no la barra espaciadora (bit 57) */
    CHECK(kbd_caps_letras("3ff0000\n") == 0);
    /* Tiene la barra (bit 57) pero no Q..P */
    CHECK(kbd_caps_letras("200000000000000\n") == 0);

    /* Lo que no se entiende no se puede afirmar: que decida el que llama abriendo el device */
    CHECK(kbd_caps_letras(NULL) == -1);
    CHECK(kbd_caps_letras("") == -1);
    CHECK(kbd_caps_letras("\n") == -1);
    CHECK(kbd_caps_letras("  \n") == -1);
    CHECK(kbd_caps_letras("sin_capacidades\n") == -1);

    /* Un dispositivo inexistente no tiene sysfs: -1 (y no "no es teclado") */
    CHECK(kbd_caps_letras_de_dev("/dev/input/event999") == -1);
    CHECK(kbd_caps_letras_de_dev(NULL) == -1);
}

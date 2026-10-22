/*
 * Interfaz de captura de logs del test de persistencia (issue #213):
 * tests/native/db_test_stubs.c implementa _lv_log_add() (el sink de LV_LOG_*)
 * y el test afirma sobre lo capturado.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

/* Vacia el buffer y el contador de lineas. */
void db_test_log_reset(void);

/* Todo lo capturado, separado por '\n' (vacio si no hubo nada). */
const char *db_test_log_text(void);

/* Cuantas lineas de log se capturaron. */
int db_test_log_lines_count(void);

/* Cuantas veces aparece `needle` en lo capturado (para probar que no hay spam). */
int db_test_log_occurrences(const char *needle);

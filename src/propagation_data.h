/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Registro IARU Region 2 de radiofaros de 10 m
 *
 *  Datos congelados: el blob sale de la lista oficial que publica la IARU R2
 *  ("RegionTwo10mBCNs.ods", las hojas ACTIVE Reg 2 10m BCNS e INactive 10m BCNS,
 *  https://storage.iaru-r2.org/index.php/s/mDCqpdaf8AXt3An). Va empaquetado en el
 *  firmware: la app Propagacion no consulta internet. Se re-genera con
 *  tools/gen_propagation_iaru.py (el generador descarta los indicativos que no
 *  entran en 10 caracteres y los marcadores que no son radiofaros: TENTATIVE y
 *  K7RA(SK)).
 *
 *  Cada registro ocupa PROPAGATION_IARU_REC bytes, sin separadores:
 *
 *      call[10]     indicativo, rellenado con espacios a la derecha
 *      freq_hz[8]   frecuencia en Hz, con ceros a la izquierda
 *      active[1]    '1' = la lista lo da por activo, '0' = inactivo / QRT / QSY
 *
 *  Los registros estan ordenados por frecuencia ascendente (y por indicativo a
 *  igual frecuencia). Ver src/propagation_data.c (el blob) y src/propagation.c
 *  (quien lo decodifica), y tests/native/test_propagation.c, que fija el largo,
 *  las puntas y los indicativos largos del registro.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PROPAGATION_IARU_CALL_LEN   10
#define PROPAGATION_IARU_REC        (PROPAGATION_IARU_CALL_LEN + 8 + 1)

/* Blob (sin el NUL del final: el largo util es `propagation_iaru_blob_len`). */
extern const char     propagation_iaru_blob[];
extern const unsigned propagation_iaru_blob_len;

#ifdef __cplusplus
}
#endif

/*
 * Test nativo de la politica del squelch (issue #166).
 *
 * Cubre dos cosas:
 *   1. La funcion PURA sql_should_enable() (0 = abierto, >0 = habilitado).
 *   2. El CAMINO de radio: sql_policy_apply() llama al setter del modo y al
 *      enable con el valor correcto, en el orden nivel->enable, y UNA sola vez
 *      por cambio (los stubs de tests/native/sql_policy_stubs.c cuentan).
 *
 * Ademas, un contrato de fuente: radio_tick() (el camino por tick) NO puede
 * llamar a la politica del squelch; si alguien la metiera ahi, el enable se
 * re-escribiria en cada tick.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sql_policy.h"
#include "sql_policy_stubs.h"
#include "test_framework.h"

/* --- 1. funcion pura ------------------------------------------------------- */

static void test_should_enable_pura(void) {
    CHECK(sql_should_enable(0) == false);

    CHECK(sql_should_enable(1) == true);
    CHECK(sql_should_enable(2) == true);
    CHECK(sql_should_enable(8) == true);
    CHECK(sql_should_enable(100) == true);
    CHECK(sql_should_enable(255) == true);   /* el rango lo limita la UI, no la politica */
}

/* --- 2. camino de radio (stubs del control) -------------------------------- */

static void test_voz_nivel_cero_abre(void) {
    sql_stub_reset();
    sql_policy_apply(x6200_mode_usb, 0);

    CHECK(sql_stub_sql_calls() == 1);
    CHECK(sql_stub_last_sql() == 0);
    CHECK(sql_stub_sql_fm_calls() == 0);
    CHECK(sql_stub_enable_calls() == 1);
    CHECK(sql_stub_last_enable() == false);
    CHECK(strcmp(sql_stub_sequence(), "ve") == 0);   /* nivel y despues enable(off) */
}

static void test_voz_nivel_positivo_habilita(void) {
    sql_stub_reset();
    sql_policy_apply(x6200_mode_usb, 8);

    CHECK(sql_stub_sql_calls() == 1);
    CHECK(sql_stub_last_sql() == 8);
    CHECK(sql_stub_sql_fm_calls() == 0);
    CHECK(sql_stub_enable_calls() == 1);
    CHECK(sql_stub_last_enable() == true);
    CHECK(strcmp(sql_stub_sequence(), "vE") == 0);
}

static void test_fm_usa_su_setter(void) {
    sql_stub_reset();
    sql_policy_apply(x6200_mode_nfm, 5);

    CHECK(sql_stub_sql_calls() == 0);
    CHECK(sql_stub_sql_fm_calls() == 1);
    CHECK(sql_stub_last_sql_fm() == 5);
    CHECK(sql_stub_enable_calls() == 1);
    CHECK(sql_stub_last_enable() == true);
    CHECK(strcmp(sql_stub_sequence(), "fE") == 0);

    sql_stub_reset();
    sql_policy_apply(x6200_mode_wfm, 0);

    CHECK(sql_stub_sql_fm_calls() == 1);
    CHECK(sql_stub_last_sql_fm() == 0);
    CHECK(sql_stub_enable_calls() == 1);
    CHECK(sql_stub_last_enable() == false);
    CHECK(strcmp(sql_stub_sequence(), "fe") == 0);
}

/* Todos los modos que no son FM usan el setter de voz. */
static void test_modos_no_fm(void) {
    static const x6200_mode_t modos[] = {
        x6200_mode_lsb, x6200_mode_lsb_dig, x6200_mode_usb, x6200_mode_usb_dig,
        x6200_mode_cw, x6200_mode_cwr, x6200_mode_am, x6200_mode_sam,
    };

    for (size_t i = 0; i < sizeof(modos) / sizeof(modos[0]); i++) {
        sql_stub_reset();
        sql_policy_apply(modos[i], 3);
        CHECK(sql_stub_sql_calls() == 1);
        CHECK(sql_stub_sql_fm_calls() == 0);
        CHECK(sql_stub_last_sql() == 3);
        CHECK(sql_stub_last_enable() == true);
    }
}

/* Transiciones 0 <-> >0: un enable por cambio, con el valor correcto, en orden.
 * Esta es la regresion del #166: el enable no se habilita nunca. */
static void test_transiciones_una_por_cambio(void) {
    sql_stub_reset();

    sql_policy_apply(x6200_mode_usb, 0);     /* abierto */
    CHECK(sql_stub_enable_calls() == 1 && sql_stub_last_enable() == false);

    sql_policy_apply(x6200_mode_usb, 8);     /* habilitado */
    CHECK(sql_stub_enable_calls() == 2 && sql_stub_last_enable() == true);

    sql_policy_apply(x6200_mode_usb, 0);     /* vuelve a abrir */
    CHECK(sql_stub_enable_calls() == 3 && sql_stub_last_enable() == false);

    sql_policy_apply(x6200_mode_usb, 100);   /* habilitado */
    CHECK(sql_stub_enable_calls() == 4 && sql_stub_last_enable() == true);

    sql_policy_apply(x6200_mode_usb, 0);     /* abierto */
    CHECK(sql_stub_enable_calls() == 5 && sql_stub_last_enable() == false);

    /* 5 cambios -> 5 llamadas al nivel y 5 al enable, ni una mas. */
    CHECK(sql_stub_sql_calls() == 5);
    CHECK(sql_stub_enable_calls() == 5);
    CHECK(strcmp(sql_stub_sequence(), "vevEvevEve") == 0);
}

/* --- 3. contrato de fuente: radio_tick no toca el squelch ------------------ */

/* Devuelve true si el cuerpo de la funcion `name` (hasta la llave de cierre que
 * balancea la de apertura) contiene el texto `needle`. Parser minimo de llaves;
 * alcanza para radio.c (sin llaves dentro de literales en radio_tick). */
static bool function_body_has(const char *src, const char *name, const char *needle) {
    const char *p = strstr(src, name);
    if (p == NULL) {
        return false;
    }
    p = strchr(p, '{');
    if (p == NULL) {
        return false;
    }

    int depth = 0;
    const char *end = p;
    for (; *end != '\0'; end++) {
        if (*end == '{') {
            depth++;
        } else if (*end == '}') {
            depth--;
            if (depth == 0) {
                end++;
                break;
            }
        }
    }

    size_t len = (size_t)(end - p);
    char *body = malloc(len + 1);
    if (body == NULL) {
        return false;
    }
    memcpy(body, p, len);
    body[len] = '\0';
    bool found = strstr(body, needle) != NULL;
    free(body);
    return found;
}

static void test_radio_tick_no_toca_sql(void) {
#ifndef SQL_RADIO_C_PATH
    CHECK(false);   /* el build tiene que pasar -DSQL_RADIO_C_PATH=... */
#else
    FILE *f = fopen(SQL_RADIO_C_PATH, "r");
    CHECK(f != NULL);
    if (f == NULL) {
        return;
    }

    CHECK(fseek(f, 0, SEEK_END) == 0);
    long n = ftell(f);
    CHECK(n > 0);
    rewind(f);

    char *buf = malloc((size_t)n + 1);
    CHECK(buf != NULL);
    if (buf == NULL) {
        fclose(f);
        return;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);

    /* radio_tick es el camino por tick: no puede aplicar el squelch. */
    CHECK(function_body_has(buf, "bool radio_tick()", "sql_policy_apply") == false);
    CHECK(function_body_has(buf, "bool radio_tick()", "x6200_control_sql") == false);

    /* El observer si lo aplica (una vez por cambio). */
    CHECK(function_body_has(buf, "on_change_cur_sql_level", "sql_policy_apply") == true);

    free(buf);
#endif
}

void test_sql_policy(void) {
    test_should_enable_pura();
    test_voz_nivel_cero_abre();
    test_voz_nivel_positivo_habilita();
    test_fm_usa_su_setter();
    test_modos_no_fm();
    test_transiciones_una_por_cambio();
    test_radio_tick_no_toca_sql();
}

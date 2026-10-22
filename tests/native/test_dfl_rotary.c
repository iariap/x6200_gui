/*
 * Native test: motor de la rueda del editor DFL (issue #173, correccion del
 * owner sobre el editor mergeado en 39539f93).
 *
 * Fija lo que src/dfl_rotary.c decide al girar la rueda sobre el parametro
 * seleccionado, contra los setters REALES (espejo de src/cfg/mode.c, ver
 * dfl_rotary_stubs.c):
 *
 *   1. Pasos por modo, los mismos de src/vol.c: LOW 10 Hz; HIGH 50 Hz y 10 Hz
 *      en CW/CWR; BW 20 Hz. Incluye el LOW negativo en CW/CWR.
 *   2. Clamps y validacion: HIGH <= MAX_FILTER_FREQ, HIGH > LOW, LOW >= 0 en
 *      SSB. El rechazo no cambia el Subject.
 *   3. LOW no es editable en AM/SAM/NFM/WFM: la rueda no lo toca.
 *   4. BW ilegal: el par LOW/HIGH resultante se valida ANTES de escribir; si no
 *      es legal, el BW queda como estaba.
 *   5. El valor aplicado SIEMPRE sale del Subject (mensaje y retorno): un
 *      rechazo deja el valor confirmado, nunca el pedido (sin valor optimista).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "dfl_rotary.h"

#include "dfl_rotary_probe.h"

#include "test_framework.h"

#include <string.h>

static bool msg_is(const char *want) {
    return strcmp(dfl_test_last_msg(), want) == 0;
}

/* 1. Pasos por modo. */
static void test_steps_per_mode(void) {
    /* SSB: LOW 10 Hz, HIGH 50 Hz. */
    dfl_test_reset(x6200_mode_usb_dig, 225, 3525, 3300);
    CHECK(dfl_rotary_apply(DFL_SEL_LOW, 1) == 230);
    CHECK(msg_is("DFL LOW 230 Hz"));
    CHECK(dfl_rotary_apply(DFL_SEL_LOW, 1) == 240);

    dfl_test_reset(x6200_mode_lsb_dig, 225, 3525, 3300);
    CHECK(dfl_rotary_apply(DFL_SEL_HIGH, 1) == 3550);   /* 3575 -> cuantiza a 3550 */
    CHECK(msg_is("DFL HIGH 3550 Hz"));
    CHECK(dfl_rotary_apply(DFL_SEL_HIGH, -1) == 3500);  /* 3500 -> cuantiza a 3500 */

    /* CW/CWR: HIGH 10 Hz. */
    dfl_test_reset(x6200_mode_cw, -250, 250, 500);
    CHECK(dfl_rotary_apply(DFL_SEL_HIGH, 1) == 260);
    CHECK(msg_is("DFL HIGH 260 Hz"));

    dfl_test_reset(x6200_mode_cwr, -250, 250, 500);
    CHECK(dfl_rotary_apply(DFL_SEL_HIGH, -1) == 240);

    /* BW 20 Hz. */
    dfl_test_reset(x6200_mode_usb_dig, 225, 3525, 3300);
    CHECK(dfl_rotary_apply(DFL_SEL_BW, 1) == 3320);
    CHECK(msg_is("DFL BW 3320 Hz"));
    CHECK(dfl_rotary_apply(DFL_SEL_BW, -2) == 3280);

    /* LOW negativo permitido en CW/CWR (guarda del setter). */
    dfl_test_reset(x6200_mode_cw, -250, 250, 500);
    CHECK(dfl_rotary_apply(DFL_SEL_LOW, -1) == -260);
    CHECK(msg_is("DFL LOW -260 Hz"));

    /* El valor guardado que no es multiplo no se redondea al abrir; la rueda si
     * cuantiza: LOW=225 +10 = 235 -> 230. */
    dfl_test_reset(x6200_mode_usb_dig, 225, 3525, 3300);
    CHECK(dfl_rotary_apply(DFL_SEL_LOW, 1) == 230);
    CHECK(dfl_test_low() == 230);
}

/* 2. Clamps y validacion (el rechazo no cambia el Subject). */
static void test_clamps(void) {
    /* HIGH en el tope: +50 pasa de MAX, la guarda rechaza y el valor queda. */
    dfl_test_reset(x6200_mode_usb_dig, 225, MAX_FILTER_FREQ, 3300);
    CHECK(dfl_rotary_apply(DFL_SEL_HIGH, 1) == MAX_FILTER_FREQ);
    CHECK(dfl_test_high() == MAX_FILTER_FREQ);
    CHECK(msg_is("DFL HIGH 10000 Hz"));

    /* HIGH sube hasta el tope exacto y no mas. */
    dfl_test_reset(x6200_mode_usb_dig, 225, 9950, 3300);
    CHECK(dfl_rotary_apply(DFL_SEL_HIGH, 1) == MAX_FILTER_FREQ);
    CHECK(dfl_rotary_apply(DFL_SEL_HIGH, 1) == MAX_FILTER_FREQ);

    /* HIGH no puede quedar <= LOW. */
    dfl_test_reset(x6200_mode_usb_dig, 3000, 3050, 3300);
    CHECK(dfl_rotary_apply(DFL_SEL_HIGH, -1) == 3050);   /* 3000 no es > low=3000 */
    CHECK(dfl_test_high() == 3050);
    CHECK(msg_is("DFL HIGH 3050 Hz"));

    /* LOW no puede ser negativo en SSB. */
    dfl_test_reset(x6200_mode_usb_dig, 0, 3000, 3300);
    CHECK(dfl_rotary_apply(DFL_SEL_LOW, -1) == 0);
    CHECK(dfl_test_low() == 0);
    CHECK(msg_is("DFL LOW 0 Hz"));
}

/* 3. LOW no editable en AM/SAM/NFM/WFM: la rueda no lo toca. */
static void test_low_not_editable(void) {
    const x6200_mode_t modes[] = { x6200_mode_am, x6200_mode_sam, x6200_mode_nfm, x6200_mode_wfm };

    for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        dfl_test_reset(modes[i], 0, 4500, 3300);
        CHECK(dfl_rotary_apply(DFL_SEL_LOW, 1) == 0);
        CHECK(dfl_rotary_apply(DFL_SEL_LOW, -1) == 0);
        CHECK(dfl_test_low() == 0);
        CHECK(msg_is("DFL LOW 0 Hz"));

        /* El modo sigue siendo editable en HIGH y BW. */
        CHECK(dfl_rotary_apply(DFL_SEL_HIGH, 1) == 4550);
        CHECK(dfl_rotary_apply(DFL_SEL_BW, 1) == 3320);
    }
}

/* 4. BW ilegal: se valida el par LOW/HIGH antes de escribir. */
static void test_bw_illegal(void) {
    /* HIGH se iria por encima de MAX: se bloquea y el BW no cambia. */
    dfl_test_reset(x6200_mode_usb_dig, 9000, 10000, 1000);
    CHECK(dfl_rotary_apply(DFL_SEL_BW, 1) == 1000);
    CHECK(dfl_test_bw() == 1000);
    CHECK(msg_is("DFL BW 1000 Hz"));

    /* BW por debajo del paso minimo (20): se bloquea. */
    dfl_test_reset(x6200_mode_usb_dig, 225, 3525, 20);
    CHECK(dfl_rotary_apply(DFL_SEL_BW, -1) == 20);
    CHECK(dfl_test_bw() == 20);

    /* En AM/SAM/NFM/WFM el BW crece desde LOW (0): legal hasta MAX. */
    dfl_test_reset(x6200_mode_am, 0, 4500, 4500);
    CHECK(dfl_rotary_apply(DFL_SEL_BW, 1) == 4520);
    CHECK(msg_is("DFL BW 4520 Hz"));

    /* El rechazo tampoco deja un valor optimista en el mensaje. */
    dfl_test_reset(x6200_mode_usb_dig, 9500, 10000, 520);
    CHECK(dfl_rotary_apply(DFL_SEL_BW, 1) == 520);   /* 540 -> new_high 10020 > MAX */
    CHECK(dfl_test_bw() == 520);
    CHECK(msg_is("DFL BW 520 Hz"));
}

/* 5. El valor aplicado sale del Subject: el retorno y el mensaje coinciden con
 * el Subject DESPUES de aplicar, tanto en el exito como en el rechazo. */
static void test_value_from_subject(void) {
    /* Exito: LOW 225 -> 230 y el Subject quedo 230. */
    dfl_test_reset(x6200_mode_usb_dig, 225, 3525, 3300);
    int32_t applied = dfl_rotary_apply(DFL_SEL_LOW, 1);
    CHECK(applied == dfl_test_low());
    CHECK(applied == 230);

    /* Rechazo: el pedido era 10050 y el Subject (10000) manda. */
    dfl_test_reset(x6200_mode_usb_dig, 225, 10000, 3300);
    applied = dfl_rotary_apply(DFL_SEL_HIGH, 1);
    CHECK(applied == dfl_test_high());
    CHECK(applied == 10000);
    CHECK(msg_is("DFL HIGH 10000 Hz"));

    /* LOW en AM: el setter devuelve 0 y el Subject tambien es 0. */
    dfl_test_reset(x6200_mode_am, 0, 4500, 3300);
    applied = dfl_rotary_apply(DFL_SEL_LOW, 1);
    CHECK(applied == dfl_test_low());
    CHECK(applied == 0);
}

void test_dfl_rotary(void) {
    test_steps_per_mode();
    test_clamps();
    test_low_not_editable();
    test_bw_illegal();
    test_value_from_subject();
}

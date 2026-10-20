/*
 * Native test: modelo puro del editor DFL (issue #173, PR 2).
 *
 * Fija lo que el editor inline (src/dfl_inline.c) decide sin LVGL ni Subjects:
 *
 *   1. Los pasos de edicion de src/vol.c (LOW 10 Hz; HIGH 50 Hz y 10 Hz en
 *      CW/CWR; BW 20 Hz) y la cuantizacion compatible con util.cpp::align_int().
 *   2. Que LOW no sea editable en AM/SAM/NFM/WFM (la regla de
 *      cfg_mode_set_low_filter()) y que AM/SAM/NFM/WFM sean simetricos.
 *   3. El GOLDEN de geometria del grafico y de los handles: el eje es la
 *      frecuencia DEL FILTRO (Hz de low/high), con el cero visible, y el dominio
 *      se ajusta al modo (SSB desde 0; CW y AM simetricos). Se verifica el mapeo
 *      ida y vuelta, el recorte a los bordes y que nunca se divide por cero.
 *   4. El mapeo de BW de src/cfg/mode.c::on_cur_filter_bw_change() y la
 *      legalidad del par LOW/HIGH resultante (bloquear fuera de rango, no dejar
 *      un estado ilegal).
 *   5. El orden seguro de escritura del "restablecer predeterminado", que tiene
 *      que pasar las guardas de los dos setters reales.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "dfl_model.h"

#include "test_framework.h"

/* Ancho util del grafico del panel inline (issue #179): DFL_GRAPH_W - PAD_L -
 * PAD_R = 487 - 8 - 8 = 471 px, x0 = 8 (los margenes de src/dfl_inline.c). El
 * golden usa estos numeros. */
#define GX0 8
#define GW  471

static void test_steps(void) {
    CHECK(dfl_step_low(x6200_mode_usb_dig) == 10);
    CHECK(dfl_step_low(x6200_mode_cwr) == 10);
    CHECK(dfl_step_bw() == 20);

    CHECK(dfl_step_high(x6200_mode_usb_dig) == 50);
    CHECK(dfl_step_high(x6200_mode_lsb) == 50);
    CHECK(dfl_step_high(x6200_mode_am) == 50);
    CHECK(dfl_step_high(x6200_mode_cw) == 10);
    CHECK(dfl_step_high(x6200_mode_cwr) == 10);
}

static void test_sel(void) {
    /* Paso de la rueda por parametro: los mismos de src/vol.c. */
    CHECK(dfl_sel_step(DFL_SEL_LOW, x6200_mode_usb_dig) == 10);
    CHECK(dfl_sel_step(DFL_SEL_HIGH, x6200_mode_usb_dig) == 50);
    CHECK(dfl_sel_step(DFL_SEL_HIGH, x6200_mode_lsb) == 50);
    CHECK(dfl_sel_step(DFL_SEL_HIGH, x6200_mode_cw) == 10);
    CHECK(dfl_sel_step(DFL_SEL_HIGH, x6200_mode_cwr) == 10);
    CHECK(dfl_sel_step(DFL_SEL_BW, x6200_mode_cw) == 20);

    /* Seleccion inicial: LOW donde es editable; en AM/SAM/NFM/WFM, HIGH. */
    CHECK(dfl_default_sel(x6200_mode_usb_dig) == DFL_SEL_LOW);
    CHECK(dfl_default_sel(x6200_mode_lsb_dig) == DFL_SEL_LOW);
    CHECK(dfl_default_sel(x6200_mode_cw) == DFL_SEL_LOW);
    CHECK(dfl_default_sel(x6200_mode_cwr) == DFL_SEL_LOW);

    CHECK(dfl_default_sel(x6200_mode_am) == DFL_SEL_HIGH);
    CHECK(dfl_default_sel(x6200_mode_sam) == DFL_SEL_HIGH);
    CHECK(dfl_default_sel(x6200_mode_nfm) == DFL_SEL_HIGH);
    CHECK(dfl_default_sel(x6200_mode_wfm) == DFL_SEL_HIGH);
}

static void test_low_editable(void) {
    CHECK(dfl_mode_low_editable(x6200_mode_usb_dig) == true);
    CHECK(dfl_mode_low_editable(x6200_mode_lsb) == true);
    CHECK(dfl_mode_low_editable(x6200_mode_cw) == true);
    CHECK(dfl_mode_low_editable(x6200_mode_cwr) == true);

    CHECK(dfl_mode_low_editable(x6200_mode_am) == false);
    CHECK(dfl_mode_low_editable(x6200_mode_sam) == false);
    CHECK(dfl_mode_low_editable(x6200_mode_nfm) == false);
    CHECK(dfl_mode_low_editable(x6200_mode_wfm) == false);

    CHECK(dfl_mode_is_symmetric(x6200_mode_am) == true);
    CHECK(dfl_mode_is_symmetric(x6200_mode_sam) == true);
    CHECK(dfl_mode_is_symmetric(x6200_mode_nfm) == true);
    CHECK(dfl_mode_is_symmetric(x6200_mode_wfm) == true);
    CHECK(dfl_mode_is_symmetric(x6200_mode_usb_dig) == false);
    CHECK(dfl_mode_is_symmetric(x6200_mode_cw) == false);
}

static void test_quantize(void) {
    /* Un valor guardado que NO es multiplo del paso (LOW=225) no se redondea
     * hasta que hay una modificacion: +10 -> 235 -> 230; -10 -> 215 -> 210. */
    CHECK(dfl_quantize(225, 10) == 220);
    CHECK(dfl_quantize(225 + 10, 10) == 230);
    CHECK(dfl_quantize(225 - 10, 10) == 210);

    CHECK(dfl_quantize(3525, 50) == 3500);
    CHECK(dfl_quantize(3525 + 50, 50) == 3550);
    CHECK(dfl_quantize(3300, 20) == 3300);
    CHECK(dfl_quantize(3310, 20) == 3300);

    /* CW: valores negativos, misma semantica que align_int (trunca hacia cero). */
    CHECK(dfl_quantize(-250, 10) == -250);
    CHECK(dfl_quantize(-255, 10) == -250);
    CHECK(dfl_quantize(-265, 10) == -260);

    /* Paso invalido: no toca el valor (nunca divide por cero). */
    CHECK(dfl_quantize(123, 0) == 123);
    CHECK(dfl_quantize(123, -5) == 123);
}

static void test_domain(void) {
    /* Adaptativo (issue #179, C.3): el dominio se ajusta al filtro con los
     * dominios "agradables" 1000 / 2000 / 5000 / 10000. */
    CHECK(dfl_domain_at(0) == 1000);
    CHECK(dfl_domain_at(1) == 2000);
    CHECK(dfl_domain_at(2) == 5000);
    CHECK(dfl_domain_at(3) == 10000);

    /* GOLDEN del contrato: USB-D 225..3525 -> 0..5000. */
    CHECK(dfl_fit_span(225, 3525) == 5000);
    dfl_domain_t ssb = dfl_domain_fit(x6200_mode_usb_dig, 225, 3525);
    CHECK(ssb.min_hz == 0);
    CHECK(ssb.max_hz == 5000);

    /* Otros filtros eligen el menor dominio que los contiene. */
    CHECK(dfl_fit_span(0, 900) == 1000);
    CHECK(dfl_fit_span(0, 1900) == 2000);
    CHECK(dfl_fit_span(50, 2950) == 5000);
    CHECK(dfl_fit_span(50, 9000) == 10000);

    /* LSB-D espejo: mismo dominio [0, M] (LOW no negativo). */
    dfl_domain_t lsb = dfl_domain_fit(x6200_mode_lsb, 225, 3525);
    CHECK(lsb.min_hz == 0);
    CHECK(lsb.max_hz == 5000);

    /* CW/CWR y simetricos: [-M, +M] con M de la lista que contiene el extremo
     * (incluso con LOW negativo). */
    dfl_domain_t cw = dfl_domain_fit(x6200_mode_cw, -250, 250);
    CHECK(cw.min_hz == -1000);
    CHECK(cw.max_hz == 1000);

    dfl_domain_t am = dfl_domain_fit(x6200_mode_am, 0, 4500);
    CHECK(am.min_hz == -5000);
    CHECK(am.max_hz == 5000);

    /* Histéresis de ampliacion: nunca reduce; solo crece si el filtro se fue
     * afuera del dominio fijado. */
    CHECK(dfl_domain_grow(5000, 225, 3525) == 5000);   /* adentro: no cambia */
    CHECK(dfl_domain_grow(5000, 225, 6000) == 10000);  /* afuera: crece */
    CHECK(dfl_domain_grow(10000, 225, 3525) == 10000); /* nunca reduce */
    CHECK(dfl_domain_grow(1000, -250, 250) == 1000);
}

/* GOLDEN: geometria del grafico y de los handles. */
static void test_graph_golden(void) {
    dfl_domain_t ssb = dfl_domain_fit(x6200_mode_usb_dig, 225, 3525);   /* [0,5000] */

    /* USB-D low=225 high=3525, dominio [0,5000], w=471: x = 8 + round(hz*471/5000). */
    CHECK(dfl_graph_x(0, ssb, GX0, GW) == 8);
    CHECK(dfl_graph_x(225, ssb, GX0, GW) == 29);      /* round(21.195) = 21 */
    CHECK(dfl_graph_x(3525, ssb, GX0, GW) == 340);    /* round(332.055)= 332 */
    CHECK(dfl_graph_x(5000, ssb, GX0, GW) == 479);    /* borde derecho */

    /* El handle de LOW cae a la izquierda del de HIGH. */
    CHECK(dfl_graph_x(225, ssb, GX0, GW) < dfl_graph_x(3525, ssb, GX0, GW));

    /* CW low=-250 high=250, dominio [-1000,1000], w=471:
     * x = 8 + round((hz+1000)*471/2000). */
    dfl_domain_t cw = dfl_domain_fit(x6200_mode_cw, -250, 250);
    CHECK(dfl_graph_x(0, cw, GX0, GW) == 244);        /* centro */
    CHECK(dfl_graph_x(-250, cw, GX0, GW) == 185);
    CHECK(dfl_graph_x(250, cw, GX0, GW) == 302);
    CHECK(dfl_graph_x(-1000, cw, GX0, GW) == 8);
    CHECK(dfl_graph_x(1000, cw, GX0, GW) == 479);

    /* AM high=4500, dominio [-5000,5000]. */
    dfl_domain_t am = dfl_domain_fit(x6200_mode_am, 0, 4500);
    CHECK(dfl_graph_x(4500, am, GX0, GW) == 455);     /* round(447.45)=447 */
    CHECK(dfl_graph_x(-4500, am, GX0, GW) == 32);     /* round(23.55) = 24 */

    /* Ida y vuelta: el mapeo es de pixeles (un paso vale ~span/w Hz), asi que se
     * exige estar a menos de un pixel de distancia, no la igualdad exacta. */
    int32_t back;
    back = dfl_graph_hz(dfl_graph_x(225, ssb, GX0, GW), ssb, GX0, GW);
    CHECK(back >= 225 - 12 && back <= 225 + 12);
    back = dfl_graph_hz(dfl_graph_x(3525, ssb, GX0, GW), ssb, GX0, GW);
    CHECK(back >= 3525 - 12 && back <= 3525 + 12);
    back = dfl_graph_hz(dfl_graph_x(-250, cw, GX0, GW), cw, GX0, GW);
    CHECK(back >= -250 - 6 && back <= -250 + 6);
    back = dfl_graph_hz(dfl_graph_x(250, cw, GX0, GW), cw, GX0, GW);
    CHECK(back >= 250 - 6 && back <= 250 + 6);

    /* Recorte a los bordes. */
    CHECK(dfl_graph_x(-99999, ssb, GX0, GW) == GX0);
    CHECK(dfl_graph_x(99999, ssb, GX0, GW) == GX0 + GW);
    CHECK(dfl_graph_hz(GX0 - 1000, ssb, GX0, GW) == 0);
    CHECK(dfl_graph_hz(GX0 + GW + 1000, ssb, GX0, GW) == 5000);

    /* Sin ancho o sin dominio: nunca divide por cero. */
    CHECK(dfl_graph_x(225, ssb, GX0, 0) == GX0);
    CHECK(dfl_graph_hz(100, ssb, GX0, 0) == 0);
    dfl_domain_t vacio = { 0, 0 };
    CHECK(dfl_graph_x(225, vacio, GX0, GW) == GX0);
    CHECK(dfl_graph_hz(100, vacio, GX0, GW) == 0);
}

/* Hit-test de los handles (issue #179): radio maximo de 22 px y, fuera de eso,
 * NINGUN handle (un toque lejano no modifica nada). */
static void test_hit_test(void) {
    /* Golden SSB: low_x=29, high_x=340. */
    CHECK(dfl_hit_test(29, 29, 340) == DFL_HANDLE_LOW);
    CHECK(dfl_hit_test(340, 29, 340) == DFL_HANDLE_HIGH);
    CHECK(dfl_hit_test(7, 29, 340) == DFL_HANDLE_LOW);      /* dl=22: al limite */
    CHECK(dfl_hit_test(6, 29, 340) == DFL_HANDLE_NONE);     /* dl=23: fuera */
    CHECK(dfl_hit_test(362, 29, 340) == DFL_HANDLE_HIGH);   /* dh=22: al limite */
    CHECK(dfl_hit_test(363, 29, 340) == DFL_HANDLE_NONE);   /* dh=23: fuera */
    CHECK(dfl_hit_test(200, 29, 340) == DFL_HANDLE_NONE);   /* lejos de ambos */
    CHECK(dfl_hit_test(0, 29, 340) == DFL_HANDLE_NONE);     /* borde del grafico */

    /* Equidistante (handles a menos de 44 px, para que ambos entren en la zona):
     * gana LOW (misma regla de desempate del contrato). */
    CHECK(dfl_hit_test(44, 29, 60) == DFL_HANDLE_LOW);      /* dl=15, dh=16 */
    CHECK(dfl_hit_test(45, 29, 60) == DFL_HANDLE_HIGH);     /* dl=16, dh=15 */

    /* Handles pegados: gana el mas cercano; muy lejos, ninguno. */
    CHECK(dfl_hit_test(50, 48, 52) == DFL_HANDLE_LOW);
    CHECK(dfl_hit_test(90, 48, 52) == DFL_HANDLE_NONE);
}

static void test_bw_targets(void) {
    int32_t lo;
    int32_t hi;

    /* SSB: recentrado (division entera, truncando hacia cero). */
    dfl_bw_targets(x6200_mode_usb_dig, 225, 3525, 3300, &lo, &hi);
    CHECK(lo == 225);
    CHECK(hi == 3525);

    dfl_bw_targets(x6200_mode_lsb, 225, 3525, 3000, &lo, &hi);
    CHECK(lo == 375);
    CHECK(hi == 3375);

    /* AM: crece desde LOW (0). */
    dfl_bw_targets(x6200_mode_am, 0, 4500, 5000, &lo, &hi);
    CHECK(lo == 0);
    CHECK(hi == 5000);

    /* CW: tambien recentrado (el case de CW esta comentado en mode.c). */
    dfl_bw_targets(x6200_mode_cw, -250, 250, 600, &lo, &hi);
    CHECK(lo == -300);
    CHECK(hi == 300);
}

static void test_bw_legal(void) {
    /* SSB: LOW no negativo, HIGH <= MAX, HIGH > LOW. */
    CHECK(dfl_bw_legal(x6200_mode_usb_dig, 375, 3375) == true);
    CHECK(dfl_bw_legal(x6200_mode_usb_dig, -8125, 11875) == false);   /* HIGH > MAX y LOW < 0 */
    CHECK(dfl_bw_legal(x6200_mode_usb_dig, -100, 2900) == false);     /* LOW < 0 en SSB */
    CHECK(dfl_bw_legal(x6200_mode_usb_dig, 3000, 3000) == false);     /* HIGH == LOW */
    CHECK(dfl_bw_legal(x6200_mode_usb_dig, 9000, 10000) == true);
    CHECK(dfl_bw_legal(x6200_mode_usb_dig, 9500, 10050) == false);    /* HIGH > MAX */

    /* CW: LOW negativo permitido. */
    CHECK(dfl_bw_legal(x6200_mode_cw, -250, 250) == true);
    CHECK(dfl_bw_legal(x6200_mode_cwr, -500, 500) == true);

    /* AM: LOW queda como estaba (0), HIGH <= MAX. */
    CHECK(dfl_bw_legal(x6200_mode_am, 0, 5000) == true);
    CHECK(dfl_bw_legal(x6200_mode_am, 0, 12000) == false);

    /* Un BW gigante en SSB se bloquea (no genera estado ilegal). */
    int32_t lo;
    int32_t hi;
    dfl_bw_targets(x6200_mode_usb_dig, 225, 3525, 20000, &lo, &hi);
    CHECK(dfl_bw_legal(x6200_mode_usb_dig, lo, hi) == false);
}

static void test_reset_order(void) {
    /* def_high > cur_low: HIGH primero (low_first=false). */
    CHECK(dfl_reset_low_first(50, 2950, 100, 200) == false);
    CHECK(dfl_reset_low_first(50, 2950, 0, 30) == false);
    CHECK(dfl_reset_low_first(0, 4500, 0, 3000) == false);

    /* def_high <= cur_low: LOW primero (si no, la guarda de set_high rechaza). */
    CHECK(dfl_reset_low_first(50, 2950, 5000, 6000) == true);
    CHECK(dfl_reset_low_first(50, 2950, 2950, 3000) == true);
    CHECK(dfl_reset_low_first(-250, 250, 2000, 4000) == true);
}

void test_dfl_editor(void) {
    test_steps();
    test_sel();
    test_low_editable();
    test_quantize();
    test_domain();
    test_graph_golden();
    test_hit_test();
    test_bw_targets();
    test_bw_legal();
    test_reset_order();
}

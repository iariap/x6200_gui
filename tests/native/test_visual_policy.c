/*
 * Native test for src/visual_policy.c (issue #14, UI-08 incremento 1).
 *
 * El modulo es puro y sin dependencias, asi que se prueba directo, sin stubs:
 *   - la matriz AUTO con hysteresis exacta en los DOS bordes de cada umbral
 *     (entrada vs salida, con el gap probado de ambos lados);
 *   - la fuente externa / carga que gana de inmediato, sin hysteresis;
 *   - el fallback conservador por datos viejos y la vuelta a AUTO;
 *   - el override manual que fija el perfil y congela el AUTO;
 *   - las lecturas invalidas que se descartan sin tocar el estado;
 *   - series que cruzan un umbral y no flapan.
 *
 * El test no lleva literales de umbral: usa los #define del header, que son el
 * contrato que el wiring del incremento 2 va a compartir.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>

#include "test_framework.h"

#include "visual_policy.h"

/* Instante fijo; la muestra es fresca (age 0) salvo en los casos de stale,
 * donde se mueve last_valid_ms. */
#define NOW 1000000u

/* Evaluacion AUTO de una muestra fresca y plausible, sin fuente externa. */
static visual_policy_result_t auto_vbat(visual_policy_t *state, uint16_t vbat_mv) {
    return visual_policy_eval(state, vbat_mv, 0, 50, false, NOW, NOW, VP_AUTO);
}

void test_visual_policy(void) {
    visual_policy_t state;
    visual_policy_result_t r;

    /* --- La tabla de umbrales sostiene la hysteresis: salir exige mas que
     * entrar y las bandas no se solapan. --- */
    CHECK(VP_BALANCED_EXIT_MV > VP_BALANCED_ENTER_MV);
    CHECK(VP_ECO_EXIT_MV > VP_ECO_ENTER_MV);
    CHECK(VP_CRITICAL_EXIT_MV > VP_CRITICAL_ENTER_MV);
    CHECK(VP_BALANCED_ENTER_MV > VP_ECO_EXIT_MV);
    CHECK(VP_ECO_ENTER_MV > VP_CRITICAL_EXIT_MV);
    CHECK(VP_STALE_FALLBACK == VP_ECO);

    /* --- Estado inicial --- */
    visual_policy_init(&state);
    CHECK(state.profile == VP_QUALITY);
    CHECK(state.output == VP_QUALITY);
    CHECK(!state.manual);
    CHECK(!state.stale);
    r = auto_vbat(&state, VP_BALANCED_EXIT_MV);
    CHECK(r.profile == VP_QUALITY);
    CHECK(!r.changed);
    CHECK(!r.stale);
    CHECK(!r.manual);
    CHECK(!r.discarded);
    CHECK(r.age_ms == 0);

    /* --- Hysteresis exacta en QUALITY <-> BALANCED --- */
    visual_policy_init(&state);
    /* un mV arriba del umbral de entrada: sigue en QUALITY */
    r = auto_vbat(&state, VP_BALANCED_ENTER_MV + 1);
    CHECK(r.profile == VP_QUALITY);
    CHECK(!r.changed);
    /* justo en el umbral de entrada: baja a BALANCED */
    r = auto_vbat(&state, VP_BALANCED_ENTER_MV);
    CHECK(r.profile == VP_BALANCED);
    CHECK(r.changed);
    CHECK(r.previous == VP_QUALITY);
    /* un mV abajo del umbral de salida: sigue en BALANCED (no flapa) */
    r = auto_vbat(&state, VP_BALANCED_EXIT_MV - 1);
    CHECK(r.profile == VP_BALANCED);
    CHECK(!r.changed);
    /* justo en el umbral de salida: vuelve a QUALITY */
    r = auto_vbat(&state, VP_BALANCED_EXIT_MV);
    CHECK(r.profile == VP_QUALITY);
    CHECK(r.changed);
    CHECK(r.previous == VP_BALANCED);

    /* --- Hysteresis exacta en BALANCED <-> ECO --- */
    visual_policy_init(&state);
    r = auto_vbat(&state, 12000);              /* dentro de BALANCED */
    CHECK(r.profile == VP_BALANCED);
    r = auto_vbat(&state, VP_ECO_ENTER_MV + 1);
    CHECK(r.profile == VP_BALANCED);
    CHECK(!r.changed);
    r = auto_vbat(&state, VP_ECO_ENTER_MV);
    CHECK(r.profile == VP_ECO);
    CHECK(r.changed);
    r = auto_vbat(&state, VP_ECO_EXIT_MV - 1);
    CHECK(r.profile == VP_ECO);
    CHECK(!r.changed);
    r = auto_vbat(&state, VP_ECO_EXIT_MV);
    CHECK(r.profile == VP_BALANCED);
    CHECK(r.changed);

    /* --- Hysteresis exacta en ECO <-> CRITICAL --- */
    visual_policy_init(&state);
    r = auto_vbat(&state, 11000);              /* dentro de ECO */
    CHECK(r.profile == VP_ECO);
    r = auto_vbat(&state, VP_CRITICAL_ENTER_MV + 1);
    CHECK(r.profile == VP_ECO);
    CHECK(!r.changed);
    r = auto_vbat(&state, VP_CRITICAL_ENTER_MV);
    CHECK(r.profile == VP_CRITICAL);
    CHECK(r.changed);
    r = auto_vbat(&state, VP_CRITICAL_EXIT_MV - 1);
    CHECK(r.profile == VP_CRITICAL);
    CHECK(!r.changed);
    r = auto_vbat(&state, VP_CRITICAL_EXIT_MV);
    CHECK(r.profile == VP_ECO);
    CHECK(r.changed);

    /* --- Fuente externa / carga: QUALITY de inmediato, sin hysteresis --- */
    visual_policy_init(&state);
    r = auto_vbat(&state, 10000);              /* CRITICAL */
    CHECK(r.profile == VP_CRITICAL);
    /* un mV por debajo del umbral de fuente externa: sigue siendo a bateria */
    r = visual_policy_eval(&state, 10000, VP_VEXT_PRESENT_MV - 1, 50, false, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_CRITICAL);
    /* en el umbral: enchufado, salta a QUALITY en la misma evaluacion */
    r = visual_policy_eval(&state, 10000, VP_VEXT_PRESENT_MV, 50, false, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_QUALITY);
    CHECK(r.changed);
    /* cargando con la fuente presente: idem */
    r = visual_policy_eval(&state, 10000, 13000, 50, true, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_QUALITY);
    /* se va la fuente: vuelve a la banda real en una sola evaluacion */
    r = auto_vbat(&state, 10000);
    CHECK(r.profile == VP_CRITICAL);
    CHECK(r.changed);

    /* --- batcap intermedio no altera la decision (manda el voltaje) --- */
    visual_policy_init(&state);
    r = visual_policy_eval(&state, 13000, 0, 50, false, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_QUALITY);
    r = visual_policy_eval(&state, 13000, 0, VP_BATCAP_MAX_PCT, false, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_QUALITY);
    CHECK(!r.discarded);
    r = visual_policy_eval(&state, 11000, 0, 50, false, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_ECO);

    /* --- Series que cruzan un umbral y no flapan --- */
    visual_policy_init(&state);
    {
        static const uint16_t series[] = {12320, 12280, 12310, 12290, 12300, 12295};
        int changes = 0;

        for (size_t i = 0; i < sizeof(series) / sizeof(series[0]); i++) {
            r = auto_vbat(&state, series[i]);
            if (r.changed) {
                changes++;
            }
        }
        CHECK(changes == 1);
        CHECK(state.profile == VP_BALANCED);
    }
    {
        static const uint16_t series[] = {11880, 11910, 11895, 11905};
        int changes = 0;

        visual_policy_init(&state);
        auto_vbat(&state, 11000);              /* ECO */
        for (size_t i = 0; i < sizeof(series) / sizeof(series[0]); i++) {
            r = auto_vbat(&state, series[i]);
            if (r.changed) {
                changes++;
            }
        }
        CHECK(changes == 1);
        CHECK(state.profile == VP_BALANCED);
    }
    {
        static const uint16_t series[] = {11290, 11305, 11295, 11310};
        int changes = 0;

        visual_policy_init(&state);
        auto_vbat(&state, 10000);              /* CRITICAL */
        for (size_t i = 0; i < sizeof(series) / sizeof(series[0]); i++) {
            r = auto_vbat(&state, series[i]);
            if (r.changed) {
                changes++;
            }
        }
        CHECK(changes == 1);
        CHECK(state.profile == VP_ECO);
    }

    /* --- Datos viejos: fallback conservador y vuelta --- */
    visual_policy_init(&state);
    /* justo en el limite de antiguedad: todavia fresco */
    r = visual_policy_eval(&state, 13000, 0, 50, false, NOW - VP_STALE_MS, NOW, VP_AUTO);
    CHECK(!r.stale);
    CHECK(r.profile == VP_QUALITY);
    CHECK(r.age_ms == VP_STALE_MS);
    /* un ms mas viejo: stale -> el voltaje no manda, cae al fallback */
    r = visual_policy_eval(&state, 13000, 0, 50, false, NOW - VP_STALE_MS - 1, NOW, VP_AUTO);
    CHECK(r.stale);
    CHECK(r.profile == VP_STALE_FALLBACK);
    CHECK(r.changed);
    CHECK(r.age_ms == VP_STALE_MS + 1);
    /* vuelve un dato valido: AUTO retoma el voltaje */
    r = visual_policy_eval(&state, 13000, 0, 50, false, NOW, NOW, VP_AUTO);
    CHECK(!r.stale);
    CHECK(r.profile == VP_QUALITY);
    CHECK(r.changed);

    /* --- Override manual: fija el perfil y congela el AUTO --- */
    visual_policy_init(&state);
    r = auto_vbat(&state, 11000);              /* AUTO: ECO */
    CHECK(r.profile == VP_ECO);
    CHECK(state.profile == VP_ECO);
    /* el usuario fija QUALITY */
    r = visual_policy_eval(&state, 11000, 0, 50, false, NOW, NOW, VP_MANUAL(VP_QUALITY));
    CHECK(r.profile == VP_QUALITY);
    CHECK(r.manual);
    CHECK(r.changed);
    CHECK(state.manual);
    /* mientras el manual manda el AUTO no avanza: 11.5 V lo habria subido a
     * BALANCED, pero la memoria del automatico sigue en ECO */
    r = visual_policy_eval(&state, 11500, 0, 50, false, NOW, NOW, VP_MANUAL(VP_QUALITY));
    CHECK(r.profile == VP_QUALITY);
    CHECK(!r.changed);
    CHECK(state.profile == VP_ECO);
    /* al soltar, el AUTO reevalua desde su memoria (ECO), no desde el manual */
    r = auto_vbat(&state, 11500);
    CHECK(!r.manual);
    CHECK(r.profile == VP_ECO);
    CHECK(r.changed);                          /* QUALITY (manual) -> ECO */
    /* el perfil manual tambien se puede mover a mano */
    r = visual_policy_eval(&state, 10000, 0, 50, false, NOW, NOW, VP_MANUAL(VP_ECO));
    CHECK(r.profile == VP_ECO);
    CHECK(r.manual);
    /* y manda sobre la staleness: se reporta vieja, pero el perfil es el elegido */
    r = visual_policy_eval(&state, 10000, 0, 50, false, NOW - VP_STALE_MS - 1, NOW,
                           VP_MANUAL(VP_ECO));
    CHECK(r.manual);
    CHECK(r.stale);
    CHECK(r.profile == VP_ECO);
    /* suelta el override con el pack bajo: el AUTO cae a CRITICAL */
    r = auto_vbat(&state, 10000);
    CHECK(!r.manual);
    CHECK(r.profile == VP_CRITICAL);
    CHECK(r.changed);

    /* --- Lecturas invalidas: se descartan sin tocar el estado --- */
    visual_policy_init(&state);
    r = auto_vbat(&state, 12000);              /* BALANCED */
    CHECK(r.profile == VP_BALANCED);
    /* voltaje por debajo del minimo */
    r = auto_vbat(&state, VP_VBAT_MIN_MV - 1);
    CHECK(r.discarded);
    CHECK(!r.changed);
    CHECK(r.profile == VP_BALANCED);
    /* voltaje por encima del maximo */
    r = auto_vbat(&state, VP_VBAT_MAX_MV + 1);
    CHECK(r.discarded);
    CHECK(r.profile == VP_BALANCED);
    /* capacidad fuera de porcentaje */
    r = visual_policy_eval(&state, 12000, 0, VP_BATCAP_MAX_PCT + 1, false, NOW, NOW, VP_AUTO);
    CHECK(r.discarded);
    CHECK(r.profile == VP_BALANCED);
    /* cargando sin fuente externa: combinacion imposible */
    r = visual_policy_eval(&state, 12000, 0, 50, true, NOW, NOW, VP_AUTO);
    CHECK(r.discarded);
    CHECK(r.profile == VP_BALANCED);
    /* la politica sigue viva: un voltaje de ECO entra a ECO */
    r = auto_vbat(&state, VP_ECO_ENTER_MV);
    CHECK(r.profile == VP_ECO);
    CHECK(r.changed);
    CHECK(!r.discarded);
    /* los bordes validos de rango no se descartan */
    r = auto_vbat(&state, VP_VBAT_MIN_MV);
    CHECK(!r.discarded);
    r = auto_vbat(&state, VP_VBAT_MAX_MV);
    CHECK(!r.discarded);
    r = visual_policy_eval(&state, 13000, 0, VP_BATCAP_MAX_PCT, false, NOW, NOW, VP_AUTO);
    CHECK(!r.discarded);

    /* --- Un estado NULL no explota (devuelve el perfil por defecto) --- */
    r = visual_policy_eval(NULL, 13000, 0, 50, false, NOW, NOW, VP_AUTO);
    CHECK(!r.changed);
    CHECK(!r.discarded);
    CHECK(r.profile == VP_QUALITY);
}

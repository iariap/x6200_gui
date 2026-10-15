/*
 * Native test for src/visual_policy.c (issue #14, UI-08 incrementos 1 a 5).
 *
 * La evaluacion es pura y sin dependencias, asi que se prueba directo, sin
 * stubs:
 *   - la matriz AUTO con hysteresis exacta en los DOS bordes de cada umbral
 *     (entrada vs salida, con el gap probado de ambos lados);
 *   - la fuente externa / carga que gana de inmediato, sin hysteresis;
 *   - el fallback conservador por datos viejos y la vuelta a AUTO;
 *   - el override manual que fija el perfil y congela el AUTO;
 *   - las lecturas invalidas que se descartan sin tocar el estado;
 *   - series que cruzan un umbral y no flapan.
 *
 * El wiring del incremento 2 (el perfil -> el modo Ahorro del waterfall) se
 * prueba contra el stub que registra la llamada a waterfall_set_battery_mode
 * (tests/native/stubs/stubs.c): cuantos llamados hubo y con que valor, sin el
 * render ni el flujo de radio. La cadencia del espectro (corrida eco-final) se
 * prueba igual, contra el stub de spectrum_set_refresh_period.
 *
 * El wiring del incremento 3 (el perfil -> el nivel del backlight) se prueba
 * igual, contra el stub que registra la llamada a backlight_set_brightness: la
 * tabla perfil -> cap, el borde exacto del cap y que el perfil nunca suba el
 * setting del usuario. Los cuatro caps declarados van como literales en la
 * tabla de casos; el resto usa los #define del header, que son el contrato que
 * el wiring comparte con el ROM y con el arnes.
 *
 * El enforcement del incremento 4 (los rewrites del tick por input y del timer
 * de idle) se prueba contra el mismo stub: el cap lo deja vigente el apply y
 * los dos rewrites lo aplican, con los dos lados del clamp comprobados sobre el
 * mismo numero (el que el stub registra).
 *
 * La persistencia del incremento 5 (el byte de params -> el override de la
 * politica y los nombres de la UI) es pura y se prueba directo. El perfil
 * vigente se registra en el apply (el que corre en el hilo de LVGL), asi que se
 * comprueba contra el stub del backlight, igual que el cap.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>
#include <string.h>

#include "test_framework.h"

#include "visual_policy.h"

/* Registro del stub de waterfall (tests/native/stubs/stubs.c). */
void test_waterfall_battery_reset(void);
bool test_waterfall_battery_mode(void);
int  test_waterfall_battery_calls(void);

/* Registro del stub de backlight (tests/native/stubs/stubs.c). */
void    test_backlight_brightness_reset(void);
int16_t test_backlight_brightness_level(void);
int     test_backlight_brightness_calls(void);

/* Registro del stub de la cadencia del espectro (tests/native/stubs/stubs.c). */
void    test_spectrum_period_reset(void);
uint8_t test_spectrum_period(void);
int     test_spectrum_period_calls(void);

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

    /* --- Wiring del incr. 2: el perfil -> el modo Ahorro del waterfall.
     * ECO y CRITICAL fuerzan el ahorro; QUALITY y BALANCED lo devuelven al
     * toggle del usuario. El stub registra cada llamada. --- */
    {
        static const struct {
            visual_profile_t profile;
            bool             forced;
        } cases[] = {
            { VP_QUALITY,  false },
            { VP_BALANCED, false },
            { VP_ECO,      true  },
            { VP_CRITICAL, true  },
        };

        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            test_waterfall_battery_reset();
            visual_policy_waterfall_apply(cases[i].profile, false);
            CHECK(test_waterfall_battery_calls() == 1);
            CHECK(test_waterfall_battery_mode() == cases[i].forced);

            /* Con el toggle del usuario prendido el modo es true en todos los
             * perfiles: el perfil no puede apagarlo. */
            test_waterfall_battery_reset();
            visual_policy_waterfall_apply(cases[i].profile, true);
            CHECK(test_waterfall_battery_calls() == 1);
            CHECK(test_waterfall_battery_mode());
        }
    }

    /* De punta a punta con la evaluacion, con el toggle del usuario apagado:
     * el que decide es el perfil. */
    visual_policy_init(&state);
    r = auto_vbat(&state, VP_BALANCED_EXIT_MV);        /* QUALITY */
    CHECK(r.profile == VP_QUALITY);
    test_waterfall_battery_reset();
    visual_policy_waterfall_apply(r.profile, false);
    CHECK(!test_waterfall_battery_mode());             /* manda el toggle */
    visual_policy_waterfall_apply(r.profile, true);
    CHECK(test_waterfall_battery_mode());              /* el toggle prendido vale */

    r = auto_vbat(&state, VP_ECO_ENTER_MV);            /* ECO */
    CHECK(r.profile == VP_ECO);
    test_waterfall_battery_reset();
    visual_policy_waterfall_apply(r.profile, false);
    CHECK(test_waterfall_battery_calls() == 1);
    CHECK(test_waterfall_battery_mode());              /* ECO fuerza el ahorro */

    r = auto_vbat(&state, VP_ECO_EXIT_MV);             /* BALANCED */
    CHECK(r.profile == VP_BALANCED);
    test_waterfall_battery_reset();
    visual_policy_waterfall_apply(r.profile, false);
    CHECK(!test_waterfall_battery_mode());             /* vuelve el toggle */

    r = auto_vbat(&state, VP_CRITICAL_ENTER_MV);       /* CRITICAL */
    CHECK(r.profile == VP_CRITICAL);
    test_waterfall_battery_reset();
    visual_policy_waterfall_apply(r.profile, false);
    CHECK(test_waterfall_battery_mode());              /* CRITICAL tambien ahorra */

    /* El fixture del banco (main-natural-eco: 11.2 V sin fuente externa) tiene
     * que caer en ECO, o el fixture deja de ejercer el wiring. El valor se
     * escribe relativo al umbral (11.2 V = VP_ECO_ENTER_MV - 200 mV) para que
     * mover la banda rompa este test en vez de dejar el fixture ciego. */
    visual_policy_init(&state);
    r = visual_policy_eval(&state, VP_ECO_ENTER_MV - 200u, 0, 76, false, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_ECO);

    /* --- Wires del espectro (cadencia ECO): mismo reparto que el waterfall, pero
     * el espectro no tiene toggle del usuario. La tabla perfil -> periodo va con el
     * literal declarado: el .c usa los #define, asi que mover el periodo deja este
     * test rojo. El stub registra cada llamada. --- */
    CHECK(VP_SPECTRUM_ECO_PERIOD == 4);
    {
        static const struct {
            visual_profile_t profile;
            uint8_t          period;
        } cases[] = {
            { VP_QUALITY,  1 },
            { VP_BALANCED, 1 },
            { VP_ECO,      4 },
            { VP_CRITICAL, 4 },
        };

        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            test_spectrum_period_reset();
            visual_policy_spectrum_apply(cases[i].profile);
            CHECK(test_spectrum_period_calls() == 1);
            CHECK(test_spectrum_period() == cases[i].period);
        }
    }

    /* Un perfil fuera del enum no ahorra: se trata como el perfil por defecto. */
    test_spectrum_period_reset();
    visual_policy_spectrum_apply((visual_profile_t) 99);
    CHECK(test_spectrum_period_calls() == 1);
    CHECK(test_spectrum_period() == 1);

    /* De punta a punta con la evaluacion: el periodo sale del pack, no de un caso
     * armado a mano. El borde de bajada (ECO -> BALANCED) vuelve a 1, asi que el
     * proximo refresco dibuja completo. */
    visual_policy_init(&state);
    r = auto_vbat(&state, VP_ECO_ENTER_MV);            /* ECO */
    CHECK(r.profile == VP_ECO);
    test_spectrum_period_reset();
    visual_policy_spectrum_apply(r.profile);
    CHECK(test_spectrum_period() == VP_SPECTRUM_ECO_PERIOD);

    r = auto_vbat(&state, VP_ECO_EXIT_MV);             /* BALANCED */
    CHECK(r.profile == VP_BALANCED);
    test_spectrum_period_reset();
    visual_policy_spectrum_apply(r.profile);
    CHECK(test_spectrum_period() == 1);

    r = auto_vbat(&state, VP_CRITICAL_ENTER_MV);       /* CRITICAL */
    CHECK(r.profile == VP_CRITICAL);
    test_spectrum_period_reset();
    visual_policy_spectrum_apply(r.profile);
    CHECK(test_spectrum_period() == VP_SPECTRUM_ECO_PERIOD);

    r = visual_policy_eval(&state, 13000, VP_VEXT_PRESENT_MV, 50, false, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_QUALITY);                    /* fuente externa */
    test_spectrum_period_reset();
    visual_policy_spectrum_apply(r.profile);
    CHECK(test_spectrum_period() == 1);

    /* --- Wiring del incr. 3: el perfil -> el nivel del backlight. La tabla
     * perfil -> cap va con los CUATRO caps declarados como literales: el .c usa
     * los #define, asi que estos numeros son el unico lugar donde mover un cap
     * deja el test rojo (los #define del header sostienen el resto). El stub
     * registra cada llamada. --- */
    CHECK(VP_BRIGHTNESS_QUALITY == VP_BRIGHTNESS_MAX);
    CHECK(VP_BRIGHTNESS_BALANCED == VP_BRIGHTNESS_MAX);
    CHECK(VP_BRIGHTNESS_ECO < VP_BRIGHTNESS_MAX);
    CHECK(VP_BRIGHTNESS_CRITICAL < VP_BRIGHTNESS_ECO);
    CHECK(VP_BRIGHTNESS_CRITICAL >= 0);
    CHECK(VP_BRIGHTNESS_OFF < 0);
    {
        static const struct {
            visual_profile_t profile;
            int16_t          cap;      /* el cap declarado por perfil */
        } cases[] = {
            { VP_QUALITY,  9 },
            { VP_BALANCED, 9 },
            { VP_ECO,      6 },
            { VP_CRITICAL, 3 },
        };

        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            /* El usuario pide el maximo: manda el cap del perfil. */
            test_backlight_brightness_reset();
            visual_policy_brightness_apply(cases[i].profile, VP_BRIGHTNESS_MAX);
            CHECK(test_backlight_brightness_calls() == 1);
            CHECK(test_backlight_brightness_level() == cases[i].cap);

            /* Un setting por debajo del cap pasa intacto: el perfil no sube. */
            test_backlight_brightness_reset();
            visual_policy_brightness_apply(cases[i].profile, 1);
            CHECK(test_backlight_brightness_calls() == 1);
            CHECK(test_backlight_brightness_level() == 1);

            /* Justo en el cap no hay recorte. */
            test_backlight_brightness_reset();
            visual_policy_brightness_apply(cases[i].profile, cases[i].cap);
            CHECK(test_backlight_brightness_level() == cases[i].cap);

            /* El usuario apago el display (VP_BRIGHTNESS_OFF): ningun perfil,
             * ni el mas austero, lo revive. */
            test_backlight_brightness_reset();
            visual_policy_brightness_apply(cases[i].profile, VP_BRIGHTNESS_OFF);
            CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_OFF);
        }
    }

    /* --- El borde del cap es exacto (con los #define del header): por encima se
     * recorta, en el cap se aplica, por debajo pasa. --- */
    test_backlight_brightness_reset();
    visual_policy_brightness_apply(VP_ECO, VP_BRIGHTNESS_ECO + 1);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO);
    visual_policy_brightness_apply(VP_ECO, VP_BRIGHTNESS_ECO);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO);
    visual_policy_brightness_apply(VP_ECO, VP_BRIGHTNESS_ECO - 1);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO - 1);
    CHECK(test_backlight_brightness_calls() == 3);

    /* --- Un perfil fuera del enum no puede indexar la tabla: se trata como el
     * perfil sin recorte (el estado por defecto de la politica). --- */
    test_backlight_brightness_reset();
    visual_policy_brightness_apply((visual_profile_t) 99, VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_calls() == 1);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_MAX);

    /* --- De punta a punta con la evaluacion: el perfil sale del pack y el nivel
     * aplicado es el recorte de ESE perfil, no de un caso armado a mano. --- */
    visual_policy_init(&state);
    r = auto_vbat(&state, VP_ECO_ENTER_MV);            /* ECO */
    CHECK(r.profile == VP_ECO);
    test_backlight_brightness_reset();
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_calls() == 1);
    CHECK(test_backlight_brightness_level() == 6);     /* el cap declarado de ECO */

    r = auto_vbat(&state, VP_ECO_EXIT_MV);             /* BALANCED */
    CHECK(r.profile == VP_BALANCED);
    test_backlight_brightness_reset();
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_ECO);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO);  /* sin recorte */

    r = auto_vbat(&state, VP_CRITICAL_ENTER_MV);       /* CRITICAL */
    CHECK(r.profile == VP_CRITICAL);
    test_backlight_brightness_reset();
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_level() == 3);     /* el cap declarado de CRITICAL */

    r = visual_policy_eval(&state, 13000, VP_VEXT_PRESENT_MV, 50, false, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_QUALITY);                    /* fuente externa */
    test_backlight_brightness_reset();
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_ECO);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO);  /* sin recorte */

    /* --- Enforcement del incr. 4: los rewrites del tick (input) y del timer
     * (idle) pasan por el cap VIGENTE. El cap lo deja el apply, que lo lee de la
     * misma tabla que el rewrite (no hay copia en backlight.c); el stub registra
     * el nivel que sale. Los dos lados del clamp tienen que dar el MISMO numero. --- */

    /* El recorte de un perfil que ahorra se ve igual desde la transicion (apply)
     * y desde el rewrite del tick: misma fuente de datos. */
    test_backlight_brightness_reset();
    visual_policy_brightness_apply(VP_ECO, VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_calls() == 1);
    CHECK(test_backlight_brightness_level() == 6);     /* lado apply */
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_calls() == 1);
    CHECK(test_backlight_brightness_level() == 6);     /* lado tick, mismo numero */

    /* Tick por input: el rewrite del nivel normal sigue recortado. */
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO);

    /* Timer de idle: el rewrite del nivel de reposo tambien pasa por el cap, y un
     * idle por debajo del cap pasa intacto (el cap solo baja, nunca sube). */
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(4);
    CHECK(test_backlight_brightness_calls() == 1);
    CHECK(test_backlight_brightness_level() == 4);
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_ECO);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO);
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_ECO - 1);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO - 1);

    /* El cap es persistente: sobrevive a todos los rewrites (no se consume ni se
     * "capta" una sola vez). */
    for (int i = 0; i < 3; i++) {
        test_backlight_brightness_reset();
        visual_policy_brightness_rewrite(VP_BRIGHTNESS_MAX);
        CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO);
    }

    /* Un display apagado (VP_BRIGHTNESS_OFF) no lo revive el cap:
     * min(-1, VP_BRIGHTNESS_ECO) = VP_BRIGHTNESS_OFF. */
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_OFF);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_OFF);

    /* Tabla del enforcement con los caps declarados como literales: el rewrite
     * del tick devuelve el cap del perfil vigente, sea cual sea. QUALITY y
     * BALANCED (sin recorte) comparten el clamp con ECO y CRITICAL (recorte): es
     * la misma politica, no una copia. */
    {
        static const struct {
            visual_profile_t profile;
            int16_t          cap;
        } cases[] = {
            { VP_QUALITY,  9 },
            { VP_BALANCED, 9 },
            { VP_ECO,      6 },
            { VP_CRITICAL, 3 },
        };

        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            visual_policy_brightness_apply(cases[i].profile, VP_BRIGHTNESS_MAX);
            test_backlight_brightness_reset();
            visual_policy_brightness_rewrite(VP_BRIGHTNESS_MAX);
            CHECK(test_backlight_brightness_calls() == 1);
            CHECK(test_backlight_brightness_level() == cases[i].cap);
        }
    }

    /* Con QUALITY/BALANCED el rewrite devuelve el setting del usuario intacto: el
     * cap es del perfil vigente, no una constante del modulo. */
    visual_policy_brightness_apply(VP_QUALITY, VP_BRIGHTNESS_MAX);
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_MAX);
    visual_policy_brightness_apply(VP_BALANCED, VP_BRIGHTNESS_MAX);
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_MAX);

    /* --- Prioridad de la fuente externa / carga: con el pack bajo el perfil es
     * QUALITY (calidad plena), asi que el cap NO restringe el rewrite aunque el
     * voltaje este en la banda de CRITICAL. --- */
    visual_policy_init(&state);
    r = auto_vbat(&state, VP_CRITICAL_ENTER_MV);       /* CRITICAL */
    CHECK(r.profile == VP_CRITICAL);
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_MAX);
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_level() == 3);     /* a bateria: recorta */

    /* Se enchufa (fuente externa): QUALITY de inmediato, sin hysteresis. */
    r = visual_policy_eval(&state, VP_CRITICAL_ENTER_MV, VP_VEXT_PRESENT_MV, 50, false,
                           NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_QUALITY);
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_MAX);
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_calls() == 1);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_MAX);  /* el cap no restringe */

    /* Idem cargando con la fuente presente: manda la presencia de fuente. */
    r = visual_policy_eval(&state, VP_CRITICAL_ENTER_MV, 13000, 50, true, NOW, NOW, VP_AUTO);
    CHECK(r.profile == VP_QUALITY);
    visual_policy_brightness_apply(r.profile, 5);
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(5);
    CHECK(test_backlight_brightness_level() == 5);

    /* Al irse la fuente vuelve la banda real y el cap recorta otra vez. */
    r = auto_vbat(&state, VP_CRITICAL_ENTER_MV);       /* CRITICAL */
    CHECK(r.profile == VP_CRITICAL);
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_MAX);
    test_backlight_brightness_reset();
    visual_policy_brightness_rewrite(VP_BRIGHTNESS_MAX);
    CHECK(test_backlight_brightness_level() == 3);

    /* --- Persistencia del override (incr. 5): el byte de params -> el override
     * de la politica. Esa conversion es TODO lo que el ROM y el arnes hacen con
     * el parametro, asi que se prueba entera: los indices del enum son manuales
     * y cualquier otro valor (VP_PARAM_AUTO incluido) cae al automatico. --- */
    CHECK(VP_PARAM_AUTO >= VP_PROFILE_COUNT);
    {
        visual_override_t override;

        for (int p = 0; p < (int) VP_PROFILE_COUNT; p++) {
            override = visual_policy_override_from_param((uint8_t) p);
            CHECK(override.manual);
            CHECK(override.profile == (visual_profile_t) p);
        }

        /* El valor reservado y cualquier byte desconocido (un equipo con otro
         * firmware, un byte corrupto) caen al automatico, nunca a un perfil
         * arbitrario. */
        override = visual_policy_override_from_param(VP_PARAM_AUTO);
        CHECK(!override.manual);
        override = visual_policy_override_from_param((uint8_t) VP_PROFILE_COUNT);
        CHECK(!override.manual);
        override = visual_policy_override_from_param((uint8_t) 200);
        CHECK(!override.manual);
        override = visual_policy_override_from_param((uint8_t) 0xFF);
        CHECK(!override.manual);
    }

    /* --- Nombres para la UI de Ajustes: uno por perfil y el del perfil por
     * defecto para lo que no es un perfil. --- */
    CHECK(strcmp(visual_policy_profile_name(VP_QUALITY), "Quality") == 0);
    CHECK(strcmp(visual_policy_profile_name(VP_BALANCED), "Balanced") == 0);
    CHECK(strcmp(visual_policy_profile_name(VP_ECO), "Eco") == 0);
    CHECK(strcmp(visual_policy_profile_name(VP_CRITICAL), "Critical") == 0);
    CHECK(strcmp(visual_policy_profile_name((visual_profile_t) 99), "Quality") == 0);

    /* --- Perfil VIGENTE (la consulta que usa la UI de Ajustes): es el ultimo
     * APLICADO, no el ultimo evaluado. Se registra en el apply porque es el que
     * corre en el hilo de LVGL (el de la evaluacion, en la radio, es el de la
     * radio): asi la lectura del dialogo no cruza hilos. --- */
    visual_policy_init(&state);
    r = auto_vbat(&state, VP_ECO_ENTER_MV);            /* ECO */
    CHECK(r.profile == VP_ECO);
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_MAX);
    CHECK(visual_policy_current() == VP_ECO);

    r = visual_policy_eval(&state, 11000, 0, 50, false, NOW, NOW, VP_MANUAL(VP_CRITICAL));
    CHECK(r.profile == VP_CRITICAL);
    CHECK(visual_policy_current() == VP_ECO);          /* todavia no se aplico */

    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_MAX);
    CHECK(visual_policy_current() == VP_CRITICAL);     /* recien ahora */

    r = auto_vbat(&state, 11000);                      /* suelta el override: vuelve ECO */
    CHECK(r.profile == VP_ECO);
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_MAX);
    CHECK(visual_policy_current() == VP_ECO);

    /* --- De punta a punta con el byte persistido: un indice fijado congela el
     * automatico (aunque el pack pida otra banda) y su cap llega al panel; el
     * valor reservado AUTO deja mandar al pack. --- */
    visual_policy_init(&state);
    r = visual_policy_eval(&state, VP_CRITICAL_ENTER_MV, 0, 50, false, NOW, NOW,
                           visual_policy_override_from_param((uint8_t) VP_ECO));
    CHECK(r.manual);
    CHECK(r.profile == VP_ECO);                        /* el pack pedia CRITICAL */
    test_backlight_brightness_reset();
    visual_policy_brightness_apply(r.profile, VP_BRIGHTNESS_MAX);
    CHECK(visual_policy_current() == VP_ECO);
    CHECK(test_backlight_brightness_calls() == 1);
    CHECK(test_backlight_brightness_level() == VP_BRIGHTNESS_ECO);

    visual_policy_init(&state);
    r = visual_policy_eval(&state, VP_CRITICAL_ENTER_MV, 0, 50, false, NOW, NOW,
                           visual_policy_override_from_param(VP_PARAM_AUTO));
    CHECK(!r.manual);
    CHECK(r.profile == VP_CRITICAL);                   /* AUTO: manda el pack */
}

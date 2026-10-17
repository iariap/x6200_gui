/*
 * Dialog-level multipass replay of the owner's mock fixtures (issue #125, section 7).
 *
 * This binary compiles the REAL src/dialog_swrscan.c with SWRSCAN_MULTIPASS_TX_ENABLED=1, so the
 * 3/5-pass path can be exercised end to end: the fixture samples are handed to the dialog exactly
 * as the radio thread hands them a flow packet (dialog_swrscan_update(vswr)) and the assertions
 * come from the owner's expected tables, not from curves invented here.
 *
 * What this test proves that the engine-level one cannot (tests/native/test_swrscan_fixtures.c):
 *  - the dialog requests EXACTLY the fixture's frequencies, in the fixture's order: 51 points per
 *    pass whatever the span, and the odd passes run backwards;
 *  - the passes button really selects 1/3/5 and the session completes by itself with the TX off
 *    and the VFO back at the app frequency;
 *  - the readout the operator reads (min, MAD, frequency, n/51) is the fixture's expected
 *    aggregate, computed by the real engine from the real samples;
 *  - the graph freezes when the session ends: no further packet moves the VFO or re-opens a
 *    session, and the LVGL teardown runs exactly once;
 *  - the fake instrument is instrumented: exactly one session start, one TX off, and every RF
 *    call goes through the stubs (there is no path to real hardware from a native test).
 *
 * Timing: the virtual clock advances at the REAL flow cadence (SWRSCAN_DWELL_MS = 35 ms per
 * packet) and the radio-thread tick (dialog_swrscan_poll) runs after every packet, so the stall
 * watchdog is exercised inside the replay. The fixture timestamps (150/210 ms) are synthetic and
 * the README is explicit that they are not an RF timing requirement, so they are NOT fed to the
 * clock: the watchdog budget stays tied to the cadence of the real flow.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_framework.h"

#include "dialog_swrscan.h"
#include "buttons.h"
#include "swr_session.h"
#include "swrscan_fixtures.h"

/* Provided by tests/native/swrscan_stubs.c. */
void    swrscan_stub_reset(void);
int     swrscan_stub_null_label(void);
int     swrscan_stub_started(void);
int     swrscan_stub_stopped(void);
void    swrscan_stub_set_fg_freq(int32_t v);
void    swrscan_stub_set_span(int32_t v);
void    swrscan_stub_fire_delete(void);
int     swrscan_stub_delete_cb_registered(void);
int32_t swrscan_stub_freq(int i);
int     swrscan_stub_freq_count(void);
int     swrscan_stub_destructed(void);
void    swrscan_stub_time_set(uint64_t t);
void    swrscan_stub_run_scheduler(void);
int     swrscan_stub_pending_jobs(void);
void    swrscan_stub_draw(void);
const char *swrscan_stub_last_label_text(void);

/* --- fixture tables -------------------------------------------------------- */

static swr_fx_sample_t    samples[SWR_FX_MAX_SAMPLES];
static int                n_samples;
static swr_fx_aggregate_t aggregates[SWR_FX_MAX_AGGREGATES];
static int                n_aggregates;
static int                fixtures_ready;

static void fixtures_load(void) {
    if (fixtures_ready) {
        return;
    }

    n_samples    = swr_fx_load_samples(samples, SWR_FX_MAX_SAMPLES);
    n_aggregates = swr_fx_load_aggregates(aggregates, SWR_FX_MAX_AGGREGATES);

    CHECK(n_samples == 1428);           /* 8 escenarios: 306 x 3 pasadas + 102 x 5 pasadas */
    CHECK(n_aggregates == 408);         /* 8 escenarios x 51 puntos */

    fixtures_ready = 1;
}

static const swr_fx_sample_t *find_sample(const char *sc, int pass, int order) {
    for (int i = 0; i < n_samples; i++) {
        if (strcmp(samples[i].scenario, sc) == 0 &&
            samples[i].pass_index == pass &&
            samples[i].order_in_pass == order) {
            return &samples[i];
        }
    }

    return NULL;
}

/* El minimo que el oraculo del owner espera para el escenario: la primera fila con la mediana mas
 * chica (las tablas vienen en orden de punto, asi que "primera" = primer indice, que es como el
 * motor resuelve los empates). */
static const swr_fx_aggregate_t *expected_min(const char *sc) {
    const swr_fx_aggregate_t *best = NULL;

    for (int i = 0; i < n_aggregates; i++) {
        if (strcmp(aggregates[i].scenario, sc) != 0) {
            continue;
        }

        if (!best || aggregates[i].median_tenths < best->median_tenths) {
            best = &aggregates[i];
        }
    }

    CHECK(best != NULL);

    return best;
}

/* --- dialog driving -------------------------------------------------------- */

static button_item_t *run_button(void) {
    return dialog_swrscan->btn_page->items[0];
}

static button_item_t *passes_button(void) {
    return dialog_swrscan->btn_page->items[3];
}

/* El dialogo es un singleton con estado estatico: cada test lo deja detenido antes de arrancar
 * (destruct_cb es el cierre real y es idempotente). */
static void dialog_quiesce(void) {
    dialog_swrscan->destruct_cb();
    swrscan_stub_reset();
}

/* "Passes:\n3 *" -> 3. */
static int passes_of(button_item_t *item) {
    const char *s = item->label_fn();
    int         v = 0;

    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            v = v * 10 + (*s - '0');
        }
    }

    return v;
}

typedef struct {
    int      passes;
    int      points;      /* puntos realmente medidos (51 x pasadas, o menos si hubo Stop) */
    uint32_t center;
    int32_t  span;
    uint32_t sim_ms;      /* tiempo VIRTUAL consumido: no hay sleeps reales */
} replay_t;

/* Reproduce un escenario por el camino real del dialogo. `stop_after` > 0 corta con el Stop del
 * operador despues de esa cantidad de puntos. */
static replay_t replay(const char *sc, int passes, int stop_after) {
    replay_t r = { .passes = passes, .points = SWR_SESSION_POINTS * passes };

    fixtures_load();
    dialog_quiesce();

    uint32_t lo       = UINT32_MAX;
    uint32_t hi       = 0;
    int      max_pass = 0;

    for (int i = 0; i < n_samples; i++) {
        if (strcmp(samples[i].scenario, sc) != 0) {
            continue;
        }

        if (samples[i].freq_hz < lo) {
            lo = samples[i].freq_hz;
        }
        if (samples[i].freq_hz > hi) {
            hi = samples[i].freq_hz;
        }
        if (samples[i].pass_index + 1 > max_pass) {
            max_pass = samples[i].pass_index + 1;
        }
    }

    CHECK(max_pass >= passes);
    CHECK(lo < hi);

    r.span   = (int32_t) (hi - lo);
    r.center = lo + (uint32_t) (r.span / 2);

    swrscan_stub_time_set(1000);
    swrscan_stub_set_fg_freq((int32_t) r.center);
    swrscan_stub_set_span(r.span);

    dialog_swrscan->construct_cb(NULL);

    button_item_t *passes_btn = passes_button();
    button_item_t *run        = run_button();

    /* El boton cicla 1 -> 3 -> 5 -> 1. */
    int guard = 0;

    while (passes_of(passes_btn) != passes && guard++ < 8) {
        passes_btn->press(passes_btn);
    }

    CHECK(passes_of(passes_btn) == passes);

    run->press(run);
    CHECK(swrscan_stub_started() == 1);         /* la unica sesion que se abre */

    int fed = 0;

    for (int p = 0; p < passes; p++) {
        for (int o = 0; o < SWR_SESSION_POINTS; o++) {
            const swr_fx_sample_t *s = find_sample(sc, p, o);

            CHECK(s != NULL);

            if (!s) {
                r.points = fed;
                return r;
            }

            /* La frecuencia que el dialogo acaba de pedir para este punto: la grilla y el sentido
             * de la pasada tienen que coincidir con el fixture (las impares barren al reves). */
            CHECK(swrscan_stub_freq(fed) == (int32_t) s->freq_hz);

            /* Un paquete del flow por punto, al ritmo real (~35 ms): asi el watchdog corre en el
             * replay y no corta una pasada normal. */
            swrscan_stub_time_set(1000 + (uint64_t) (fed + 1) * SWRSCAN_DWELL_MS);
            dialog_swrscan_update((float) s->raw_tenths * 0.1f);
            dialog_swrscan_poll();
            fed++;

            if (stop_after > 0 && fed == stop_after) {
                r.points = fed;
                r.sim_ms = (uint32_t) fed * SWRSCAN_DWELL_MS;
                run->press(run);                /* Stop del operador */
                return r;
            }
        }
    }

    r.sim_ms = (uint32_t) fed * SWRSCAN_DWELL_MS;

    return r;
}

/* La sesion termino sola y el instrumento fake lo confirma. */
static void check_completed(const replay_t *r) {
    CHECK(strcmp(dialog_swrscan_state_text(), "Completed") == 0);

    CHECK(swrscan_stub_started() == 1);                     /* una sola sesion, un solo TX on */
    CHECK(swrscan_stub_stopped() == 1);                     /* un solo TX off (idempotente) */
    CHECK(swrscan_stub_freq_count() == r->points + 1);       /* 51 x pasadas + la vuelta al centro */
    CHECK(swrscan_stub_freq(r->points) == (int32_t) r->center);
    CHECK(swrscan_stub_pending_jobs() == 1);                /* el cierre de LVGL, encolado una vez */

    /* Tiempo simulado: exactamente pasadas x el presupuesto de UNA pasada (51 x dwell). El
     * presupuesto NO se toco; la sesion multipasada lo multiplica por la cantidad de pasadas. */
    CHECK(r->sim_ms == (uint32_t) r->passes * SWRSCAN_TX_BUDGET_MS);
    CHECK(SWRSCAN_TX_BUDGET_MS == 1785);
}

/* El readout que lee el operador es el agregado esperado del owner, punto por punto. */
static void check_readout(const char *sc, int passes) {
    const swr_fx_aggregate_t *best = expected_min(sc);

    if (!best) {
        return;
    }

    swrscan_stub_draw();

    uint16_t mhz = (uint16_t) (best->freq_hz / 1000000);
    uint16_t khz = (uint16_t) ((best->freq_hz / 1000) % 1000);
    uint16_t hz  = (uint16_t) (best->freq_hz % 1000);

    char expect[96];

    if (passes > 1) {
        snprintf(expect, sizeof(expect), "min %.1f +/- %.1f @ %u.%03u.%03u kHz (n=51/51)",
                 best->median_tenths / 10.0, best->mad_tenths / 10.0,
                 (unsigned) mhz, (unsigned) khz, (unsigned) hz);
    } else {
        /* Con una sola pasada no hay dispersion: el motor no inventa un MAD. */
        snprintf(expect, sizeof(expect), "min %.1f @ %u.%03u.%03u kHz (n=51/51)",
                 best->median_tenths / 10.0, (unsigned) mhz, (unsigned) khz, (unsigned) hz);
    }

    CHECK(strcmp(swrscan_stub_last_label_text(), expect) == 0);

    printf("  %-22s %d pasada(s): %s\n", sc, passes, swrscan_stub_last_label_text());
}

/* La grafica queda congelada: el job de LVGL cierra una vez y mas paquetes no la mueven. */
static void check_frozen(void) {
    button_item_t *run = run_button();

    swrscan_stub_run_scheduler();
    CHECK(strcmp(run->label, "Run") == 0);

    int before = swrscan_stub_freq_count();

    dialog_swrscan_update(1.2f);
    dialog_swrscan_update(1.2f);

    CHECK(swrscan_stub_freq_count() == before);     /* ningun VFO nuevo */
    CHECK(swrscan_stub_started() == 1);             /* ninguna sesion nueva */
    CHECK(swrscan_stub_stopped() == 1);             /* ningun TX de mas */
    CHECK(strcmp(dialog_swrscan_state_text(), "Completed") == 0);
}

/* --- tests ----------------------------------------------------------------- */

/* Escenario 1 del issue: valle idealizado, 3 pasadas, 100 kHz / 51 puntos. */
void test_swrscan_mp_single_valley(void) {
    CHECK(SWRSCAN_MULTIPASS_TX_ENABLED == 1);       /* este binario SI habilita el camino */

    replay_t r = replay("single_valley_3pass", 3, 0);

    CHECK(r.points == 3 * SWR_SESSION_POINTS);
    CHECK(r.span == 100000);
    CHECK(r.center == 7100000);

    check_completed(&r);
    check_readout("single_valley_3pass", 3);
    check_frozen();
}

/* Escenario 2: outlier aislado en una pasada (1.0 en el punto 6) y otro alto (4.6 en el 25). La
 * mediana de 5 los descarta: el minimo reportado sigue siendo 1.2 @ 7.100.000, nunca 1.0. */
void test_swrscan_mp_outliers_5pass(void) {
    replay_t r = replay("outliers_5pass", 5, 0);

    CHECK(r.points == 5 * SWR_SESSION_POINTS);

    check_completed(&r);
    check_readout("outliers_5pass", 5);
    check_frozen();
}

/* Zoom: el mismo valle con 4 spans distintos (50 / 200 / 500 kHz). Los 51 puntos no cambian: lo
 * que cambia es el paso. wide_zoom_3pass es SOLO mock (su ventana de 500 kHz centrada en 7.100
 * MHz se sale de la banda de 40 m): aca no se transmite nada real, el instrumento es un stub. */
void test_swrscan_mp_zoom_spans(void) {
    static const struct {
        const char *sc;
        int32_t     span_hz;
        int32_t     step_hz;
    } table[] = {
        { "narrow_zoom_3pass",  50000,  1000 },
        { "medium_zoom_3pass", 200000,  4000 },
        { "wide_zoom_3pass",   500000, 10000 },
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        replay_t r = replay(table[i].sc, 3, 0);

        CHECK(r.span == table[i].span_hz);
        CHECK(r.span / (SWR_SESSION_POINTS - 1) == table[i].step_hz);
        CHECK(r.center == 7100000);

        check_completed(&r);
        check_readout(table[i].sc, 3);
        check_frozen();
    }

    /* El span angosto resuelve el valle en 7.103.000; el ancho (10 kHz de paso) lo pierde y el
     * minimo se va al borde: es la informacion que el operador necesita para decidir que span
     * usar, y sale del dato medido. */
    CHECK(expected_min("narrow_zoom_3pass")->freq_hz == 7103000);
    CHECK(expected_min("wide_zoom_3pass")->median_tenths == 35);
}

/* Escenarios 5 y 6: dos valles reales (no forzar un unico ajuste global) y todos los puntos
 * iguales (no hay una resonancia unica creible). */
void test_swrscan_mp_double_and_flat(void) {
    replay_t r = replay("double_valley_3pass", 3, 0);

    check_completed(&r);
    check_readout("double_valley_3pass", 3);
    check_frozen();

    r = replay("flat_3pass", 3, 0);

    check_completed(&r);
    check_readout("flat_3pass", 3);
    check_frozen();

    /* En flat_3pass todas las medianas son iguales: el minimo que se reporta es el primer punto
     * (7.050.000), no una resonancia inventada. El MAD chico dice que la lectura es estable. */
    CHECK(expected_min("flat_3pass")->freq_hz == 7050000);
    CHECK(expected_min("flat_3pass")->median_tenths == 26);
    CHECK(expected_min("flat_3pass")->mad_tenths == 1);
}

/* Escenario 3: la resonancia se mueve entre pasadas (7.092 -> 7.108 MHz). El minimo agregado es
 * 1.8 y su MAD 0.5: el readout lo muestra como inestable en vez de fingir un valle firme. */
void test_swrscan_mp_moving_5pass(void) {
    replay_t r = replay("moving_5pass", 5, 0);

    check_completed(&r);
    check_readout("moving_5pass", 5);
    check_frozen();

    CHECK(expected_min("moving_5pass")->median_tenths == 18);
    CHECK(expected_min("moving_5pass")->mad_tenths == 5);
    CHECK(expected_min("moving_5pass")->freq_hz == 7096000);
}

/* Escenario 8 del issue: el operador corta a mitad de la tercera pasada. La sesion queda marcada
 * como parcial, con lo medido, el TX apagado y el cierre de LVGL encolado una sola vez. */
void test_swrscan_mp_stop_partial(void) {
    replay_t r = replay("single_valley_3pass", 3, 2 * SWR_SESSION_POINTS + 20);

    CHECK(r.points == 2 * SWR_SESSION_POINTS + 20);

    CHECK(strcmp(dialog_swrscan_state_text(), "Stopped (parcial)") == 0);
    CHECK(swrscan_stub_started() == 1);
    CHECK(swrscan_stub_stopped() == 1);
    CHECK(swrscan_stub_null_label() == 0);

    /* El teardown es idempotente: destruct_cb + el LV_EVENT_DELETE que sigue no apagan dos veces
     * (el invariante del #85, ahora con una sesion multipasada a medio camino). */
    dialog_swrscan->destruct_cb();
    swrscan_stub_fire_delete();
    CHECK(swrscan_stub_stopped() == 1);
    CHECK(swrscan_stub_null_label() == 0);

    /* Lo medido no se pierde: los 51 puntos de las dos pasadas completas siguen ahi. */
    swrscan_stub_draw();
    CHECK(strstr(swrscan_stub_last_label_text(), "n=51/51") != NULL);
}

/* Escenario 9 del issue: la misma medicion con 1, 3 y 5 pasadas. Los numeros salen del fixture;
 * lo que se compara es cuanto cambia el minimo agregado, NO se afirma que mas pasadas sea
 * siempre mejor (moving_5pass muestra lo contrario: mas pasadas exponen inestabilidad real). */
void test_swrscan_mp_passes_comparison(void) {
    replay_t one = replay("single_valley_3pass", 1, 0);

    CHECK(one.points == SWR_SESSION_POINTS);
    check_completed(&one);

    swrscan_stub_draw();

    char one_txt[96];
    snprintf(one_txt, sizeof(one_txt), "%s", swrscan_stub_last_label_text());

    check_frozen();

    replay_t three = replay("single_valley_3pass", 3, 0);

    check_completed(&three);

    swrscan_stub_draw();

    char three_txt[96];
    snprintf(three_txt, sizeof(three_txt), "%s", swrscan_stub_last_label_text());

    check_frozen();

    replay_t five = replay("outliers_5pass", 5, 0);

    check_completed(&five);

    swrscan_stub_draw();

    char five_txt[96];
    snprintf(five_txt, sizeof(five_txt), "%s", swrscan_stub_last_label_text());

    check_frozen();

    /* Misma grilla (100 kHz / 51 puntos) y misma frecuencia central en los tres casos. */
    CHECK(one.span == three.span && three.span == five.span);
    CHECK(one.center == three.center && three.center == five.center);
    CHECK(one.center == 7100000);

    /* 1 pasada: no hay MAD (una sola muestra por punto). 3 y 5: el MAD aparece. */
    CHECK(strstr(one_txt, "+/-") == NULL);
    CHECK(strstr(three_txt, "+/-") != NULL);
    CHECK(strstr(five_txt, "+/-") != NULL);

    /* 1 vs 3: el minimo de una sola pasada es mas bajo (ruido de +-0.1 por pasada) que la mediana
     * de tres, que sube al valor estable 1.2. Es exactamente el efecto que el issue pide. */
    CHECK(strstr(one_txt, "min 1.1") != NULL);
    CHECK(strstr(three_txt, "min 1.2") != NULL);
    CHECK(strstr(five_txt, "min 1.2") != NULL);

    /* El outlier de 1.0 del punto 6 en outliers_5pass NO aparece como minimo con 5 pasadas. */
    CHECK(strstr(five_txt, "min 1.0") == NULL);

    printf("  comparacion 1/3/5 pasadas (100 kHz, 51 puntos, misma frecuencia central):\n");
    printf("    1 pasada : %s\n", one_txt);
    printf("    3 pasadas: %s\n", three_txt);
    printf("    5 pasadas: %s\n", five_txt);
}

/* El instrumento fake: contadores exactos de comandos y cero RF real. La sesion multipasada mas
 * larga (5 pasadas) emite 255 frecuencias + la vuelta al centro, enciende el TX una vez y lo
 * apaga una vez; el tiempo total es virtual. */
void test_swrscan_mp_instrumentation(void) {
    replay_t r = replay("outliers_5pass", 5, 0);

    check_completed(&r);

    CHECK(swrscan_stub_freq_count() == 256);
    CHECK(swrscan_stub_freq(0) == 7050000);         /* primer punto de la grilla */
    CHECK(swrscan_stub_freq(50) == 7150000);        /* ultimo de la 1ra pasada (ascendente) */
    CHECK(swrscan_stub_freq(51) == 7150000);        /* la 2da pasada arranca en el borde alto */
    CHECK(swrscan_stub_freq(255) == 7100000);       /* vuelta al centro de la app */
    CHECK(r.sim_ms == 5 * SWRSCAN_TX_BUDGET_MS);

    check_frozen();
}

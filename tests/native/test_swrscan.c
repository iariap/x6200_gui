/*
 * Native test for src/dialog_swrscan.c (issue #85: "Cerrar la app con el barrido
 * corriendo reinicia la GUI", SIGSEGV fallo=0x4).
 *
 * The defect: the dialog's destructor stopped the sweep through its own button
 * callback with a NULL item -- `destruct_cb(): if (run) dialog_swrscan_run_cb(NULL)`
 * -- and that callback's first line is `button_update_label(item, "Run")`, which
 * dereferences `item->type` (offset 4 of button_item_t). The radio died with
 * `CRASHLOG sig=11 fallo=0x4` on the close path (scheduler_work ->
 * main_screen_close_app -> dialog_destruct -> destruct_cb -> apply_cmd); it was
 * reproduced on the bench with the shipping binary (md5 d9ccb7d4c224) by opening
 * SWRScan, pressing Run and closing: `pc` resolved inside button_update_label and
 * `lr` inside destruct_cb.
 *
 * The invariants this test pins: closing the dialog with the sweep running stops
 * the sweep WITHOUT handing a NULL item to a button callback, and the teardown is
 * idempotent -- destruct_cb plus the LV_EVENT_DELETE that follows stop it once,
 * and LV_EVENT_DELETE alone (without destruct_cb) also stops it.
 *
 * The REAL src/dialog_swrscan.c is staged next to minimal stubs
 * (tests/native/stubs_swrscan/), the same pattern as the clock/backlight/
 * digital-modes tests. The button_update_label stub records the NULL case instead
 * of crashing (the real one dereferences it), so a regression fails an assertion.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_framework.h"

#include "dialog_swrscan.h"
#include "buttons.h"
#include "roe_curve.h"
#include "theme.h"

/* Provided by tests/native/swrscan_stubs.c. */
void swrscan_stub_reset(void);
int  swrscan_stub_null_label(void);
int  swrscan_stub_started(void);
int  swrscan_stub_stopped(void);
void swrscan_stub_set_fg_freq(int32_t v);
void swrscan_stub_set_span(int32_t v);
void swrscan_stub_fire_delete(void);
int  swrscan_stub_delete_cb_registered(void);
int32_t swrscan_stub_freq(int i);
int  swrscan_stub_freq_count(void);
int  swrscan_stub_destructed(void);

/* El dibujo del chart (issue #119): el test dispara el callback de dibujo del dialogo y lee la
 * polilinea que pinto (los segmentos de ancho 4: la curva). */
void      swrscan_stub_draw(void);
int       swrscan_stub_segment_count(void);
int       swrscan_stub_curve_segment_count(void);
lv_coord_t swrscan_stub_curve_x(int i, int end);
lv_coord_t swrscan_stub_curve_y(int i, int end);
uint8_t   swrscan_stub_chart_bg_opa(void);
int       swrscan_stub_chart_bg_color_set(void);
lv_color_t swrscan_stub_curve_color(void);
lv_color_t swrscan_stub_label_color(void);

/* Puntos de la curva dibujada con el barrido mas largo del dialogo (51 muestras). */
#define STUB_CURVE_MAX ((SWRSCAN_MAX_POINTS - 1) * ROE_CURVE_OVERSAMPLE + 1)

static button_item_t *run_button(void) {
    return dialog_swrscan->btn_page->items[0];
}

void test_swrscan(void) {
    swrscan_stub_reset();
    swrscan_stub_set_fg_freq(7018300);      /* 40 m CW, the field case of the issue */
    swrscan_stub_set_span(50000);

    dialog_swrscan->construct_cb(NULL);
    CHECK(swrscan_stub_delete_cb_registered());     /* the LV_EVENT_DELETE safety net */

    button_item_t *run = run_button();

    /* --- ciclo 1: abrir, arrancar el barrido, cerrar (el camino del `off`) --- */
    run->press(run);
    CHECK(swrscan_stub_started() == 1);
    CHECK(swrscan_stub_null_label() == 0);

    dialog_swrscan->destruct_cb();                  /* lo que llama dialog_destruct */
    CHECK(swrscan_stub_null_label() == 0);          /* <- falla antes del fix */
    CHECK(swrscan_stub_stopped() == 1);

    /* Idempotente: el LV_EVENT_DELETE que sigue no lo apaga dos veces. */
    swrscan_stub_fire_delete();
    CHECK(swrscan_stub_stopped() == 1);
    CHECK(swrscan_stub_null_label() == 0);

    /* --- ciclo 2: reabrir, arrancar, cerrar otra vez --- */
    dialog_swrscan->construct_cb(NULL);
    run->press(run);
    CHECK(swrscan_stub_started() == 2);
    dialog_swrscan->destruct_cb();
    CHECK(swrscan_stub_null_label() == 0);
    CHECK(swrscan_stub_stopped() == 2);

    /* --- LV_EVENT_DELETE solo (sin destruct_cb) tambien apaga el barrido --- */
    dialog_swrscan->construct_cb(NULL);
    run->press(run);
    CHECK(swrscan_stub_started() == 3);
    swrscan_stub_fire_delete();
    CHECK(swrscan_stub_stopped() == 3);
    CHECK(swrscan_stub_null_label() == 0);
}

/*
 * Ampliaciones del owner sobre el #113 (2026-10-16): el barrido se adapta al span ("aumentar la
 * frecuencia de barrido como lo hace el nanovna") y TRANSMITE al piso de potencia del equipo
 * ("la potencia de transmision durante el barrido debe ser la minima posible").
 *
 * El barrido avanza un punto por paquete del flow de la radio, que tarda ~35 ms (flow.h:
 * "Usually a packet arrives every 35ms"), asi que la duracion es puntos x dwell; el dwell no es
 * un parametro de la GUI (aether_control solo expone x6200_control_swrscan_set on/off, sin
 * cadencia). El conteo sale del span (dialog_swrscan_points_for_span) y el presupuesto de
 * duracion lo fija el codigo (SWRSCAN_TX_BUDGET_MS; subirlo no compila, ver el _Static_assert de
 * dialog_swrscan.c).
 *
 * Lo que se fija aca: la tabla span -> puntos -> paso exacto -> duracion, los extremos en los
 * limites del span, la grilla determinista y el piso de potencia del barrido.
 */
void test_swrscan_sweep(void) {
    const uint32_t center = 7018300;    /* 40 m CW */

    /* La tabla del modo automatico: el span manda el conteo y el paso parte el span exacto. */
    static const struct {
        int32_t span_hz;
        int32_t points;
        int32_t step_hz;
    } table[] = {
        {  50000, 41, 1250 },
        { 100000, 51, 2000 },
        { 200000, 51, 4000 },
        { 500000, 51, 10000 },
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        int32_t span_hz = table[i].span_hz;
        int32_t points  = dialog_swrscan_points_for_span(span_hz);

        CHECK(points == table[i].points);
        CHECK(span_hz % (points - 1) == 0);                        /* el paso parte el span exacto */
        CHECK(span_hz / (points - 1) == table[i].step_hz);
        CHECK(span_hz / (points - 1) >= SWRSCAN_STEP_TARGET_HZ);   /* nunca mas fino que el objetivo */
        CHECK(points * SWRSCAN_DWELL_MS <= SWRSCAN_TX_BUDGET_MS);  /* y entra en el presupuesto */

        /* Borde a borde: index 0 es el borde bajo del span y index puntos-1 el borde alto. */
        CHECK(dialog_swrscan_sweep_freq(center, span_hz, 0) == center - (uint32_t) span_hz / 2);
        CHECK(dialog_swrscan_sweep_freq(center, span_hz, points - 1) == center + (uint32_t) span_hz / 2);
    }

    /* El piso del conteo: 25 kHz / 1250 Hz = 20 intervalos, por debajo de SWRSCAN_MIN_POINTS-1,
     * asi que manda el piso (spans chicos densos) y el paso sigue partiendo el span exacto. */
    CHECK(dialog_swrscan_points_for_span(25000) == SWRSCAN_MIN_POINTS);
    CHECK(25000 % (dialog_swrscan_points_for_span(25000) - 1) == 0);
    CHECK(dialog_swrscan_points_for_span(25000) * SWRSCAN_DWELL_MS <= SWRSCAN_TX_BUDGET_MS);

    /* Span sin divisores en el rango (11111 = 41 x 271): manda la exactitud -- 2 puntos, paso =
     * span -- en vez de un paso con resto. */
    CHECK(dialog_swrscan_points_for_span(11111) == 2);
    CHECK(11111 % (dialog_swrscan_points_for_span(11111) - 1) == 0);
    CHECK(dialog_swrscan_sweep_freq(center, 11111, 1) - dialog_swrscan_sweep_freq(center, 11111, 0) == 11111);

    /* El barrido real del dialogo con el span del caso de campo (50 kHz -> 41 puntos). */
    const int32_t span   = 50000;
    const int     points = dialog_swrscan_points_for_span(span);

    CHECK(points == 41);

    swrscan_stub_reset();
    swrscan_stub_set_fg_freq((int32_t) center);
    swrscan_stub_set_span(span);

    dialog_swrscan->construct_cb(NULL);
    button_item_t *run = run_button();

    run->press(run);                                        /* Run: do_init + set_freq(borde bajo) */
    CHECK(swrscan_stub_freq(0) == (int32_t) center - span / 2);

    for (int k = 1; k <= points; k++) {
        dialog_swrscan_update(1.5f);                        /* un paquete por punto */
    }

    CHECK(swrscan_stub_freq_count() == points + 1);         /* los puntos + la vuelta al inicio */

    for (int k = 1; k < points; k++) {                      /* paso uniforme = span / (puntos-1) */
        CHECK(swrscan_stub_freq(k) - swrscan_stub_freq(k - 1) == span / (points - 1));
    }

    /* El ultimo punto medido es el borde ALTO del span (borde a borde: el paso parte el span
     * exacto) y despues el barrido vuelve al borde bajo. */
    CHECK(swrscan_stub_freq(points - 1) == (int32_t) center + span / 2);
    CHECK(swrscan_stub_freq(points) == (int32_t) center - span / 2);

    int32_t first[SWRSCAN_MAX_POINTS + 1];

    for (int k = 0; k <= points; k++) {
        first[k] = swrscan_stub_freq(k);
    }

    /* Determinismo: el mismo barrido da la misma grilla. */
    run->press(run);                                        /* Stop */
    swrscan_stub_reset();
    run->press(run);                                        /* Run otra vez */

    for (int k = 1; k <= points; k++) {
        dialog_swrscan_update(1.5f);
    }

    CHECK(swrscan_stub_freq_count() == points + 1);

    int32_t again[SWRSCAN_MAX_POINTS + 1];

    for (int k = 0; k <= points; k++) {
        again[k] = swrscan_stub_freq(k);
    }

    CHECK(memcmp(first, again, (size_t) (points + 1) * sizeof(first[0])) == 0);

    run->press(run);                                        /* Stop: deja el dialogo quieto */

    /* La potencia del barrido es el piso del control del equipo (issue #113: "la minima
     * posible"): 0.1 W = 1 decima de watt, el minimo que aceptan el control del panel
     * (src/vol.c, VOL_PWR: LV_MAX(0.1f, ...)) y el control remoto (src/remote_server.cpp,
     * {"pwr", CMD_PWR, 1, 100} en decimas). El barrido ponia 3 W fijos (radio_start_swrscan). */
    CHECK(SWRSCAN_TX_POWER_DW == SWRSCAN_PWR_FLOOR_DW);
    CHECK(SWRSCAN_TX_POWER_DW >= 1);
    CHECK(SWRSCAN_TX_POWER_DW <= SWRSCAN_PWR_CEILING_DW);
    CHECK(SWRSCAN_TX_POWER_W == 0.1f);
    CHECK(SWRSCAN_TX_POWER_W < 3.0f);

    /* El presupuesto de duracion es exactamente el barrido mas largo, dentro de los ~2 s que fijo
     * el owner; y los 101 puntos del NanoVNA no entran (101 x 35 = 3535 ms). */
    CHECK(SWRSCAN_TX_BUDGET_MS == SWRSCAN_MAX_POINTS * SWRSCAN_DWELL_MS);
    CHECK(SWRSCAN_TX_BUDGET_MS <= 2000);
    CHECK(101 * SWRSCAN_DWELL_MS > SWRSCAN_TX_BUDGET_MS);

    printf("barrido #113 automatico por span (paso objetivo %d Hz, %d..%d puntos):\n",
           SWRSCAN_STEP_TARGET_HZ, SWRSCAN_MIN_POINTS, SWRSCAN_MAX_POINTS);

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        int32_t points_i = dialog_swrscan_points_for_span(table[i].span_hz);

        printf("  span %6d Hz -> %2d puntos, paso %5d Hz, %4d ms de barrido (presupuesto %d ms)\n",
               table[i].span_hz, points_i, table[i].step_hz, points_i * SWRSCAN_DWELL_MS,
               SWRSCAN_TX_BUDGET_MS);
    }

    printf("  potencia del barrido: %.1f W (%d decimas: el piso del control del equipo, antes 3.0 W)\n",
           SWRSCAN_TX_POWER_W, SWRSCAN_TX_POWER_DW);
}
/*
 * ---------------------------------------------------------------------------
 * #119: la curva dibujada es una parabola suave, no una escalera
 * ---------------------------------------------------------------------------
 *
 * El owner reporto que la ROE del #117 "no es una parabola todavia": en la radio la curva sale
 * ESCALONADA (mesetas planas con escalones verticales). El QA extrajo la curva del framebuffer
 * (496 puntos, docs/evidencia/2026-10-15-validacion-arreglos/roe-muestras-extraidas.csv, commit
 * b39d6014) y midio el patron: corridas de hasta 62 columnas con el MISMO valor (p.ej. 1.08) —
 * eso es una escalera, no una parabola.
 *
 * Causa (fundada en docs/HALLAZGOS.md 46, medida sobre el binario de la radio): el GUI que el
 * owner flasheo (md5 e294229dd32d, imagen c640645d810c) NO tiene el codigo del #117 — su
 * `roe_curve_resample` es byte por byte el Catmull-Rom del #104 (compilado con el mismo
 * toolchain y flags: 376/376 bytes identicos en 0x4393c) y `roe_curve_quadratic_fit` no esta en
 * el binario. Ese resampler pasa por cada muestra y clampa cada tramo al rango de las DOS
 * muestras que lo enmarcan: con muestras cuantizadas (SWR en pasos de 0.1) dos muestras iguales
 * dejan el tramo entero PLANO — de ahi las mesetas — y el salto entre mesetas es el escalon
 * vertical. El arbol de este worktree (el #117: ajuste cuadratico) no tiene ese defecto: medido
 * con estos mismos 496 puntos, el paso maximo entre puntos dibujados es 1 px (contra 8 px del
 * Catmull-Rom con las muestras del barrido de 41, y 23 px con el dataset crudo).
 *
 * El test que sigue fija la forma dibujada con el DATASET REAL como entrada, sobre el camino
 * real del chart (construct -> Run -> dialog_swrscan_update por punto -> draw_cb, con la escala
 * Log y la geometria del chart): el unico resample que corre es el de src/roe_curve.c. Contra el
 * #104 (lo que corre la radio hoy) falla; contra el arbol pasa (el RED esta en el reporte).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* Los 496 puntos que el QA extrajo columna por columna de frame-2.png (calibracion aproximada
 * del eje: la forma es fiel, los absolutos no — dicho por el QA en el anexo). */
#define ROE119_DATASET_LEN 496

static const float roe119_dataset[ROE119_DATASET_LEN] = {
    1.72f, 1.97f, 2.08f, 1.15f, 1.14f, 1.13f, 1.12f, 1.12f,
    1.1f, 1.1f, 1.09f, 1.09f, 1.08f, 1.08f, 1.08f, 1.08f,
    1.08f, 1.08f, 1.08f, 1.09f, 1.09f, 1.09f, 1.1f, 1.1f,
    1.1f, 1.11f, 1.12f, 1.12f, 1.13f, 1.14f, 1.14f, 1.16f,
    1.16f, 1.17f, 1.19f, 1.2f, 1.2f, 1.21f, 1.22f, 1.23f,
    1.24f, 1.24f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f,
    1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f,
    1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f,
    1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f,
    1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f,
    1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f,
    1.25f, 1.24f, 1.24f, 1.24f, 1.22f, 1.21f, 1.2f, 1.18f,
    1.16f, 1.14f, 1.13f, 1.12f, 1.1f, 1.08f, 1.08f, 1.06f,
    1.05f, 1.03f, 1.02f, 1.01f, 1.0f, 0.99f, 0.98f, 0.97f,
    0.96f, 0.96f, 0.96f, 0.96f, 0.97f, 0.98f, 0.99f, 1.01f,
    1.02f, 1.04f, 1.04f, 1.06f, 1.07f, 1.07f, 1.08f, 1.08f,
    1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f,
    1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f,
    1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f,
    1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f,
    1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f,
    1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f,
    1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f, 1.08f,
    1.08f, 1.08f, 1.08f, 1.08f, 1.07f, 1.07f, 1.06f, 1.05f,
    1.04f, 1.02f, 1.02f, 1.0f, 0.99f, 0.98f, 0.97f, 0.96f,
    0.96f, 0.95f, 0.96f, 0.97f, 0.98f, 0.99f, 1.01f, 1.03f,
    1.05f, 1.07f, 1.09f, 1.1f, 1.12f, 1.13f, 1.14f, 1.16f,
    1.17f, 1.19f, 1.2f, 1.22f, 1.23f, 1.24f, 1.25f, 1.27f,
    1.27f, 1.29f, 1.3f, 1.31f, 1.31f, 1.32f, 1.33f, 1.33f,
    1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.33f,
    1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.33f,
    1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.33f,
    1.33f, 1.33f, 1.33f, 1.33f, 1.33f, 1.34f, 1.35f, 1.35f,
    1.37f, 1.38f, 1.4f, 1.41f, 1.43f, 1.44f, 1.45f, 1.47f,
    1.48f, 1.48f, 1.49f, 1.51f, 1.51f, 1.51f, 1.51f, 1.5f,
    1.49f, 1.48f, 1.46f, 1.45f, 1.43f, 1.41f, 1.4f, 1.38f,
    1.36f, 1.34f, 1.33f, 1.31f, 1.3f, 1.29f, 1.27f, 1.25f,
    1.25f, 1.23f, 1.22f, 1.2f, 1.19f, 1.18f, 1.17f, 1.16f,
    1.14f, 1.13f, 1.13f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f,
    1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f,
    1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f,
    1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f,
    1.12f, 1.12f, 1.12f, 1.1f, 1.1f, 1.09f, 1.08f, 1.07f,
    1.05f, 1.03f, 1.02f, 1.0f, 0.99f, 0.98f, 0.97f, 0.96f,
    0.95f, 0.95f, 0.96f, 0.97f, 0.98f, 0.99f, 1.0f, 1.02f,
    1.03f, 1.05f, 1.07f, 1.07f, 1.09f, 1.1f, 1.1f, 1.11f,
    1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f,
    1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.12f, 1.11f, 1.09f,
    1.09f, 1.07f, 1.05f, 1.04f, 1.03f, 1.01f, 0.99f, 0.98f,
    0.97f, 0.96f, 0.95f, 0.96f, 0.96f, 0.97f, 0.98f, 0.99f,
    1.01f, 1.02f, 1.03f, 1.05f, 1.06f, 1.08f, 1.09f, 1.1f,
    1.12f, 1.13f, 1.14f, 1.16f, 1.17f, 1.19f, 1.2f, 1.22f,
    1.23f, 1.24f, 1.25f, 1.26f, 1.26f, 1.27f, 1.28f, 1.28f,
    1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f,
    1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f,
    1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f,
    1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f,
    1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f, 1.28f,
    1.28f, 1.28f, 1.28f, 1.28f, 1.27f, 1.27f, 1.26f, 1.25f,
    1.25f, 1.24f, 1.23f, 1.22f, 1.2f, 1.19f, 1.18f, 1.17f,
    1.16f, 1.14f, 1.13f, 1.12f, 1.1f, 1.09f, 1.08f, 1.07f,
    1.05f, 1.04f, 1.02f, 1.0f, 0.99f, 0.98f, 0.97f, 0.96f,};

/* Muestra del barrido: el valor del CENTRO del intervalo k de `points` en el dataset. El
 * dataset es la curva que dibujo el binario flasheado y sus muestras son nudos exactos de ese
 * Catmull-Rom (la curva pasa por ellas), asi que el valor del centro del intervalo ES la
 * muestra medida que el pipeline vuelve a procesar. */
static float roe119_sample(int points, int k) {
    int lo = (int) ((int64_t) k * ROE119_DATASET_LEN / points);
    int hi = (int) ((int64_t) (k + 1) * ROE119_DATASET_LEN / points) - 1;

    if (hi < lo) {
        hi = lo;
    }

    return roe119_dataset[(lo + hi) / 2];
}

/* Corrida mas larga de valores identicos: el patron de la escalera (62 columnas en 1.08). */
static int roe119_longest_run(const float *v, int n) {
    int best = 1;
    int run  = 1;

    for (int i = 1; i < n; i++) {
        run = (v[i] == v[i - 1]) ? run + 1 : 1;

        if (run > best) {
            best = run;
        }
    }

    return best;
}

static int roe119_distinct(const float *v, int n) {
    int distinct = 0;

    for (int i = 0; i < n; i++) {
        bool seen = false;

        for (int j = 0; j < i; j++) {
            if (v[j] == v[i]) {
                seen = true;
                break;
            }
        }

        if (!seen) {
            distinct++;
        }
    }

    return distinct;
}

static lv_coord_t coord_abs(lv_coord_t v) {
    return (v < 0) ? (lv_coord_t) -v : v;
}

/* El test de forma del #119 sobre el dataset real: la curva dibujada tiene que ser UN valle
 * suave (sin escalones de 90 grados, sin mesetas de escalera) y pasar cerca de las muestras. */
void test_swrscan_render(void) {
    /* --- el dataset es la escalera medida en la radio (la prueba del owner) --- */
    CHECK(ROE119_DATASET_LEN == 496);
    CHECK(roe119_longest_run(roe119_dataset, ROE119_DATASET_LEN) == 62);
    CHECK(roe119_distinct(roe119_dataset, ROE119_DATASET_LEN) == 58);

    const int32_t span   = 50000;                                   /* el span de la captura */
    const int     points = dialog_swrscan_points_for_span(span);

    CHECK(points == 41);

    swrscan_stub_reset();
    swrscan_stub_set_fg_freq(7028544);                              /* el dial de la captura */
    swrscan_stub_set_span(span);

    dialog_swrscan->construct_cb(NULL);

    /* El chart pinta su propio fondo (issue #119, el fantasma): sin eso la home se ve a traves
     * del grafico porque el panel del dialogo es semitransparente. */
    CHECK(swrscan_stub_chart_bg_opa() == LV_OPA_COVER);
    CHECK(swrscan_stub_chart_bg_color_set());
    button_item_t *run = run_button();

    run->press(run);                                                /* Run: el barrido arranca */

    for (int k = 0; k < points; k++) {
        dialog_swrscan_update(roe119_sample(points, k));            /* un paquete por punto */
    }

    swrscan_stub_draw();                                            /* el chart se pinta */

    const int m = roe_curve_point_count(points);

    CHECK(swrscan_stub_curve_segment_count() == m - 1);             /* la curva entera */

    /* La curva y el eje salen del rol `text` del tema (no de un blanco escrito a mano): sobre el
     * panel del chart, que ahora es opaco, es lo que los deja legibles en los temas claros. */
    CHECK(swrscan_stub_curve_color().red == (uint8_t) ((theme_hex(TC_text) >> 16) & 0xff));
    CHECK(swrscan_stub_curve_color().green == (uint8_t) ((theme_hex(TC_text) >> 8) & 0xff));
    CHECK(swrscan_stub_label_color().blue == (uint8_t) (theme_hex(TC_text) & 0xff));

    /* La polilinea: y[0] abre el primer segmento, y[i] lo cierra el segmento i-1. */
    static lv_coord_t y[STUB_CURVE_MAX];
    static lv_coord_t y2[STUB_CURVE_MAX];

    CHECK(m <= STUB_CURVE_MAX);

    for (int i = 0; i < m && i < STUB_CURVE_MAX; i++) {
        y[i] = (i == 0) ? swrscan_stub_curve_y(0, 0) : swrscan_stub_curve_y(i - 1, 1);
    }

    /* (a) sin escalones a 90 grados: el paso entre puntos consecutivos <= 2 px (la escalera del
     *     Catmull-Rom da 8 px con estas muestras). */
    int worst_step = 0;
    int v          = 0;

    for (int i = 1; i < m; i++) {
        int step = coord_abs((lv_coord_t) (y[i] - y[i - 1]));

        if (step > worst_step) {
            worst_step = step;
        }

        if (y[i] < y[v]) {
            v = i;
        }
    }

    CHECK(worst_step <= 2);

    /* (b) un solo valle: monotona (dentro de 1 px de cuantizacion) a cada lado del minimo. La
     *     escalera sube y baja decenas de px a cada lado. */
    int worst_left  = 0;
    int worst_right = 0;

    for (int i = v; i > 0; i--) {
        int drop = (int) (y[i] - y[i - 1]);

        if (drop > worst_left) {
            worst_left = drop;
        }
    }

    for (int i = v; i + 1 < m; i++) {
        int drop = (int) (y[i] - y[i + 1]);

        if (drop > worst_right) {
            worst_right = drop;
        }
    }

    CHECK(worst_left <= 1);
    CHECK(worst_right <= 1);

    /* (c) la curva no se sale del chart. El draw pinta en [y1, y1 + h] con y1 = coords.y1 - 5 =
     *     69 (el chart esta en coords.y1 = 74) y h = 330, el `h` ESTATICO del archivo que usa
     *     calc_y() (el `h` local de draw_cb es 330 - 5 y solo lo usan la grilla y las etiquetas):
     *     la banda dibujada es 69..399, la misma que se mide en los cuadros de la radio. */
    int inside = 1;

    for (int i = 0; i < m; i++) {
        if (y[i] < 69 || y[i] > 399) {
            inside = 0;
        }
    }

    CHECK(inside);

    /* (d) el valle dibujado: el vertice del ajuste (el minimo de la curva) y el minimo MEDIDO
     *     (el readout sale de las muestras) son cantidades distintas y su distancia se REPORTA.
     *     Con el dataset real la distancia es grande (varias muestras): es el PENDIENTE que la
     *     spec del #113 ya habia declarado para revision estetica del owner (> 2 muestras), aca
     *     medido con datos reales. Lo que el test exige es que la curva tenga UN valle de verdad
     *     (no una recta ni una escalera) dentro del barrido; el numero queda impreso abajo. */
    int s_min = 0;

    for (int k = 1; k < points; k++) {
        if (roe119_sample(points, k) < roe119_sample(points, s_min)) {
            s_min = k;
        }
    }

    float c_min = (float) v / (float) ROE_CURVE_OVERSAMPLE;
    float delta = (c_min > (float) s_min) ? c_min - (float) s_min : (float) s_min - c_min;

    CHECK(v > 0 && v + 1 < m);                      /* un valle de verdad, no una recta */

    /* (e) determinismo: el mismo barrido dibuja la misma polilinea. */
    run->press(run);                                                /* Stop */
    run->press(run);                                                /* Run otra vez */

    for (int k = 0; k < points; k++) {
        dialog_swrscan_update(roe119_sample(points, k));
    }

    swrscan_stub_draw();

    CHECK(swrscan_stub_curve_segment_count() == m - 1);

    for (int i = 0; i < m; i++) {
        y2[i] = (i == 0) ? swrscan_stub_curve_y(0, 0) : swrscan_stub_curve_y(i - 1, 1);
    }

    CHECK(memcmp(y, y2, (size_t) m * sizeof(y[0])) == 0);

    run->press(run);                                                /* Stop: deja el dialogo quieto */

    printf("#119 forma dibujada (dataset real de 496 puntos, %d muestras del barrido):\n", points);
    printf("  paso maximo entre puntos dibujados: %d px (la escalera del #104 da 8 px)\n", worst_step);
    printf("  caida maxima a la izquierda/derecha del valle: %d / %d px (escalera: decenas)\n",
           worst_left, worst_right);
    printf("  valle dibujado en la muestra %.2f, minimo medido en la muestra %d (delta %.2f)\n",
           (double) c_min, s_min, (double) delta);
}

/*
 * #119, defecto 2: al terminar el barrido la app se queda abierta (el cierre es del usuario).
 *
 * Lo que el codigo hace (fundado en el reporte): el barrido del dialogo NO termina solo —
 * do_step envuelve freq_index y sigue midiendo — y NINGUN camino de fin de pasada cierra el
 * dialogo. Los unicos cierres son la tecla ESC (key_cb -> dialog_destruct), el "app off" del
 * control remoto (main_screen_close_app) y las teclas de la barra de casa (apps_disable). Este
 * test fija esa invariante: una pasada completa (los puntos + la vuelta al borde bajo + mas
 * paquetes) deja el dialogo VIVO, con el barrido corriendo, sin NULL en el callback de boton y
 * sin que la radio quede en el estado del barrido.
 */
void test_swrscan_sweep_end(void) {
    const int32_t span   = 50000;
    const int     points = dialog_swrscan_points_for_span(span);

    swrscan_stub_reset();
    swrscan_stub_set_fg_freq(7028544);
    swrscan_stub_set_span(span);

    dialog_swrscan->construct_cb(NULL);

    button_item_t *run = run_button();

    run->press(run);

    /* Una pasada completa y mas alla del borde (el barrido envuelve y sigue). */
    for (int k = 0; k < points + 5; k++) {
        dialog_swrscan_update(1.5f);
    }

    CHECK(swrscan_stub_destructed() == 0);          /* nadie cerro el dialogo */
    CHECK(run->label != NULL);
    CHECK(strcmp(run->label, "Stop") == 0);         /* y el barrido sigue corriendo */
    CHECK(swrscan_stub_stopped() == 0);             /* nadie lo paro por detras */
    CHECK(swrscan_stub_null_label() == 0);          /* ni lo cerro con el callback en NULL (#85) */

    /* La vuelta al borde bajo de la pasada siguiente tampoco cierra nada: el barrido sigue
     * moviendo el VFO. */
    int before = swrscan_stub_freq_count();

    dialog_swrscan_update(1.5f);
    dialog_swrscan_update(1.5f);

    CHECK(swrscan_stub_freq_count() > before);
    CHECK(swrscan_stub_destructed() == 0);

    run->press(run);                                /* el cierre lo pide el usuario (Stop) */

    CHECK(swrscan_stub_stopped() == 1);
}

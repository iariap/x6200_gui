/*
 * Native tests for src/dialog_swrscan.c (issues #85, #113, #119 and #125).
 *
 * #85: "Cerrar la app con el barrido corriendo reinicia la GUI" (SIGSEGV
 * fallo=0x4). The dialog's destructor stopped the sweep through its own button
 * callback with a NULL item -- `destruct_cb(): if (run) dialog_swrscan_run_cb(NULL)`
 * -- and that callback's first line is `button_update_label(item, "Run")`, which
 * dereferences `item->type` (offset 4 of button_item_t). The invariants this file
 * pins: closing the dialog with the sweep running stops the sweep WITHOUT handing
 * a NULL item to a button callback, and the teardown is idempotent -- destruct_cb
 * plus the LV_EVENT_DELETE that follows stop it once, and LV_EVENT_DELETE alone
 * (without destruct_cb) also stops it.
 *
 * #113: the sweep adapts to the span and transmits at the floor power.
 *
 * #119: the drawn curve is a smooth valley, not the staircase of the Catmull-Rom
 * resampler the flashed ROM ran; and a finished sweep does NOT close the app.
 *
 * #125: the sweep is a SESSION (src/swr_session.[ch]): 51 fixed points whatever
 * the span, the same grid every pass, per-frequency aggregation, the measured
 * minimum in the readout, and the graph frozen when the session completes. The
 * dialog is driven through its real public surface (Run button, radio packets,
 * the scheduler job that closes the LVGL side) and its collaborators are stubs.
 * The end-to-end multipass replay against the owner's fixtures lives in
 * tests/native/test_swrscan_multipass.c (that one needs the gate compiled in).
 *
 * The REAL src/dialog_swrscan.c is staged next to minimal stubs
 * (tests/native/stubs_swrscan/), the same pattern as the clock/backlight/
 * digital-modes tests.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_framework.h"

#include "dialog_swrscan.h"
#include "buttons.h"
#include "radio.h"
#include "roe_curve.h"
#include "swr_session.h"
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
int  swrscan_stub_chart_refreshes(void);
void swrscan_stub_time_set(uint64_t t);
void swrscan_stub_run_scheduler(void);
int  swrscan_stub_pending_jobs(void);
/* #133: ventana abierta / estado de radio, para las guardas del comando remoto. */
void swrscan_stub_dialog_open(void);
void swrscan_stub_dialog_other_open(void);
void swrscan_stub_dialog_close(void);
void swrscan_stub_set_radio_state(radio_state_t state);

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
const char *swrscan_stub_last_label_text(void);
/* #125: el texto que el boton de pasadas pasaria a mostrar cuando su callback pide el repintado. */
const char *swrscan_stub_refreshed_label(void);
int        swrscan_stub_rect_count(void);
lv_color_t swrscan_stub_rect_color(void);
uint8_t    swrscan_stub_rect_opa(void);

/* Puntos de la curva dibujada con el barrido del #125 (51 muestras). */
#define STUB_CURVE_MAX ((SWR_SESSION_POINTS - 1) * ROE_CURVE_OVERSAMPLE + 1)

static button_item_t *run_button(void) {
    return dialog_swrscan->btn_page->items[0];
}

static button_item_t *passes_button(void) {
    return dialog_swrscan->btn_page->items[3];
}

/* Corre una pasada completa: un paquete del flow por punto de la grilla. */
static void run_one_pass(void) {
    for (int k = 0; k < SWR_SESSION_POINTS; k++) {
        dialog_swrscan_update(1.5f);
    }
}

/* El dialogo es un singleton con estado estatico (run, sesion, grilla): cada test lo deja
 * detenido y limpio antes de arrancar. destruct_cb() es el cierre real y es idempotente (con el
 * barrido ya quieto no toca la radio ni la memoria), asi que sirve de quiesce. */
static void dialog_quiesce(void) {
    dialog_swrscan->destruct_cb();
    swrscan_stub_dialog_close();      /* #133: la ventana deja de ser la actual */
    swrscan_stub_reset();
}

void test_swrscan(void) {
    dialog_quiesce();
    swrscan_stub_set_fg_freq(7018300);      /* 40 m CW, the field case of the issue */
    swrscan_stub_set_span(50000);

    dialog_swrscan->construct_cb(NULL);
    CHECK(swrscan_stub_delete_cb_registered());     /* the LV_EVENT_DELETE safety net */

    button_item_t *run = run_button();

    /* --- ciclo 1: abrir, arrancar el barrido, cerrar (el camino del `off`) --- */
    run->press(run);
    CHECK(swrscan_stub_started() == 1);
    CHECK(swrscan_stub_null_label() == 0);

    /* #133: cerrar con el barrido corriendo NO pide repintar el chart: el objeto se esta
     * destruyendo (issue #85, no dibujar sobre objetos muertos). */
    int refreshes = swrscan_stub_chart_refreshes();

    dialog_swrscan->destruct_cb();                  /* lo que llama dialog_destruct */
    CHECK(swrscan_stub_null_label() == 0);          /* <- falla antes del fix */
    CHECK(swrscan_stub_stopped() == 1);
    CHECK(swrscan_stub_chart_refreshes() == refreshes);

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
 * #125: 51 puntos por barrido, independientes del span; el span solo cambia el paso. El
 * presupuesto de TX es el de UNA pasada (51 x dwell) y NO se sube; el barrido sigue
 * transmitiendo al piso de potencia del equipo (issue #113).
 *
 * El barrido avanza un punto por paquete del flow de la radio, que tarda ~35 ms (flow.h:
 * "Usually a packet arrives every 35ms"), asi que la duracion es puntos x dwell; el dwell no es
 * un parametro de la GUI (aether_control solo expone x6200_control_swrscan_set on/off, sin
 * cadencia). El presupuesto de duracion lo fija el codigo (SWRSCAN_TX_BUDGET_MS; subirlo no
 * compila, ver el _Static_assert de dialog_swrscan.c).
 */
void test_swrscan_sweep(void) {
    const uint32_t center = 7018300;    /* 40 m CW */

    /* Los 4 spans del dialogo: mismo conteo (51), paso adaptado (1/2/4/10 kHz). */
    static const struct {
        int32_t span_hz;
        int32_t step_hz;
    } table[] = {
        {  50000,  1000 },
        { 100000,  2000 },
        { 200000,  4000 },
        { 500000, 10000 },
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        int32_t span_hz = table[i].span_hz;

        CHECK(span_hz % (SWR_SESSION_POINTS - 1) == 0);              /* el paso parte el span exacto */
        CHECK(span_hz / (SWR_SESSION_POINTS - 1) == table[i].step_hz);
        CHECK(SWR_SESSION_POINTS * SWRSCAN_DWELL_MS <= SWRSCAN_TX_BUDGET_MS);

        /* El barrido real del dialogo con ese span: 51 frecuencias, borde a borde. */
        dialog_quiesce();
        swrscan_stub_set_fg_freq((int32_t) center);
        swrscan_stub_set_span(span_hz);

        dialog_swrscan->construct_cb(NULL);

        button_item_t *run = run_button();

        run->press(run);
        CHECK(swrscan_stub_freq(0) == (int32_t) center - span_hz / 2);

        run_one_pass();

        /* 51 puntos + la vuelta a la frecuencia de la app (freq_center) al completar. */
        CHECK(swrscan_stub_freq_count() == SWR_SESSION_POINTS + 1);

        for (int k = 1; k < SWR_SESSION_POINTS; k++) {               /* paso uniforme */
            CHECK(swrscan_stub_freq(k) - swrscan_stub_freq(k - 1) == table[i].step_hz);
        }

        CHECK(swrscan_stub_freq(SWR_SESSION_POINTS - 1) == (int32_t) center + span_hz / 2);
        CHECK(swrscan_stub_freq(SWR_SESSION_POINTS) == (int32_t) center);

        /* La sesion completa sola: el dialogo queda abierto, el TX apagado y el boton vuelve a
         * Run cuando el hilo de LVGL corre el job encolado. */
        CHECK(swrscan_stub_destructed() == 0);
        CHECK(swrscan_stub_stopped() == 1);
        CHECK(strcmp(run->label, "Stop") == 0);                      /* todavia no corrio el job */
        swrscan_stub_run_scheduler();
        CHECK(strcmp(run->label, "Run") == 0);

        dialog_swrscan->destruct_cb();                               /* deja el dialogo quieto */
    }

    /* Determinismo: el mismo barrido da la misma grilla. */
    dialog_quiesce();
    swrscan_stub_set_fg_freq((int32_t) center);
    swrscan_stub_set_span(50000);

    dialog_swrscan->construct_cb(NULL);

    button_item_t *run = run_button();

    run->press(run);
    run_one_pass();

    int32_t first[SWR_SESSION_POINTS + 1];

    for (int k = 0; k <= SWR_SESSION_POINTS; k++) {
        first[k] = swrscan_stub_freq(k);
    }

    swrscan_stub_run_scheduler();                           /* el cierre LVGL del barrido */
    swrscan_stub_reset();
    run->press(run);                                        /* Run otra vez */
    run_one_pass();

    CHECK(swrscan_stub_freq_count() == SWR_SESSION_POINTS + 1);

    for (int k = 0; k <= SWR_SESSION_POINTS; k++) {
        CHECK(swrscan_stub_freq(k) == first[k]);
    }

    swrscan_stub_run_scheduler();
    dialog_swrscan->destruct_cb();

      /* Potencia del barrido: 3,0 W desde el 27/10 (medido en la radio: a 0,1 W el detector no
       * mide en 15/10 m y el flow entrega ROE 1.0 sin potencia; ver dialog_swrscan.h). Sigue
       * dentro del control del equipo (piso 0,1 W, techo 8,0 W) y por debajo del ATU. */
      CHECK(SWRSCAN_TX_POWER_DW == 30);
      CHECK(SWRSCAN_TX_POWER_DW >= SWRSCAN_PWR_FLOOR_DW);
      CHECK(SWRSCAN_TX_POWER_DW <= SWRSCAN_PWR_CEILING_DW);
      CHECK(SWRSCAN_TX_POWER_W == 3.0f);
      CHECK(SWRSCAN_TX_POWER_W < (float) (SWRSCAN_PWR_CEILING_DW / 10));

    /* El presupuesto de duracion es exactamente el barrido de UNA pasada, dentro de los ~2 s que
     * fijo el owner; y los 101 puntos del NanoVNA no entran (101 x 35 = 3535 ms). */
    CHECK(SWRSCAN_TX_BUDGET_MS == SWR_SESSION_POINTS * SWRSCAN_DWELL_MS);
    /* Subido de 1785 a 3570 el 27/10 (decision del owner; evidencia en dialog_swrscan.h):
     * el dwell real medido en la radio es 48,4 ms de media y 63 ms peor caso. */
    CHECK(SWRSCAN_TX_BUDGET_MS <= 4000);
    CHECK(101 * SWRSCAN_DWELL_MS > SWRSCAN_TX_BUDGET_MS);

    printf("barrido #125: %d puntos por span, paso adaptado, presupuesto %d ms de UNA pasada\n",
           SWR_SESSION_POINTS, SWRSCAN_TX_BUDGET_MS);

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        printf("  span %6d Hz -> %2d puntos, paso %5d Hz, %4d ms de barrido\n",
               table[i].span_hz, SWR_SESSION_POINTS, table[i].step_hz,
               SWR_SESSION_POINTS * SWRSCAN_DWELL_MS);
    }

    printf("  potencia del barrido: %.1f W (%d decimas: el piso del control del equipo)\n",
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
 * vertical. El arbol de este worktree (el #117: ajuste cuadratico) no tiene ese defecto.
 *
 * El test que sigue fija la forma dibujada con el DATASET REAL como entrada, sobre el camino
 * real del chart (construct -> Run -> dialog_swrscan_update por punto -> draw_cb, con la escala
 * Log y la geometria del chart): el unico resample que corre es el de src/roe_curve.c.
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

/* La muestra que el flow de la radio entrega de verdad: cuantizada a 0.1 (pack->vswr es un byte
 * en decimas, radio.c hace pack->vswr * 0.1f) y con el piso fisico de ROE 1.0 (ROE < 1 no existe;
 * el #125 la declara invalida y NUNCA la convierte en curva, ver test_swr_session y los fixtures).
 * El dataset extraido del framebuffer trae 44 valores por debajo de 1.0 (artefactos de la
 * calibracion del eje que hizo el QA: la forma es fiel, los absolutos no — anexo del #119), asi
 * que se saturan al piso. Cuantizar ADEMAS hace la entrada MAS escalonada (6 valores distintos,
 * corridas de 7 identicos): es el peor caso para el resample, y la curva dibujada tiene que
 * seguir siendo un valle suave. */
static float roe119_feed(float v) {
    float q = roundf(v * 10.0f) / 10.0f;

    return (q < 1.0f) ? 1.0f : q;
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
    const int     points = SWR_SESSION_POINTS;

    /* La entrada que recibe el pipeline (cuantizada a 0.1 + piso 1.0) tambien es una escalera:
     * pocos valores distintos y corridas largas de identicos. */
    float fed[SWR_SESSION_POINTS];

    for (int k = 0; k < points; k++) {
        fed[k] = roe119_feed(roe119_sample(points, k));
    }

    CHECK(roe119_distinct(fed, points) == 6);
    CHECK(roe119_longest_run(fed, points) >= 5);

    dialog_quiesce();
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
        dialog_swrscan_update(fed[k]);                              /* un paquete por punto */
    }

    swrscan_stub_draw();                                            /* el chart se pinta */

    const int m = roe_curve_point_count(points);

    CHECK(swrscan_stub_curve_segment_count() == m - 1);             /* la curva entera */

    /* La curva y el eje salen del rol `text` del tema (no de un blanco escrito a mano): sobre el
     * panel del chart, que ahora es opaco, es lo que los deja legibles en los temas claros. */
    CHECK(swrscan_stub_curve_color().red == (uint8_t) ((theme_hex(TC_text) >> 16) & 0xff));
    CHECK(swrscan_stub_curve_color().green == (uint8_t) ((theme_hex(TC_text) >> 8) & 0xff));
    CHECK(swrscan_stub_label_color().blue == (uint8_t) (theme_hex(TC_text) & 0xff));

    /* El estado y el readout (issue #125) van sobre UN rect del color del panel del chart y
     * opacidad parcial: el texto cae encima de la grilla, de las etiquetas del eje y de la
     * curva, y sin fondo quedaba ilegible (lo mostro la primera captura del arnes). */
    CHECK(swrscan_stub_rect_count() == 1);
    CHECK(swrscan_stub_rect_color().red == (uint8_t) ((theme_hex(TC_panel) >> 16) & 0xff));
    CHECK(swrscan_stub_rect_opa() == LV_OPA_80);

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
        if (fed[k] < fed[s_min]) {
            s_min = k;
        }
    }

    float c_min = (float) v / (float) ROE_CURVE_OVERSAMPLE;
    float delta = (c_min > (float) s_min) ? c_min - (float) s_min : (float) s_min - c_min;

    CHECK(v > 0 && v + 1 < m);                      /* un valle de verdad, no una recta */

    /* (e) el minimo MEDIDO del readout sale del dato agregado, no del vertice del ajuste. */
    CHECK(strstr(dialog_swrscan_state_text(), "Completed") != NULL);
    CHECK(strstr(swrscan_stub_last_label_text(), "min ") != NULL);

    /* (f) determinismo: el mismo barrido dibuja la misma polilinea. */
    swrscan_stub_run_scheduler();                   /* el cierre LVGL: run vuelve a false */

    swrscan_stub_reset();
    swrscan_stub_set_fg_freq(7028544);
    swrscan_stub_set_span(span);
    dialog_swrscan->construct_cb(NULL);

    run = run_button();
    run->press(run);

    for (int k = 0; k < points; k++) {
        dialog_swrscan_update(fed[k]);
    }

    swrscan_stub_draw();

    CHECK(swrscan_stub_curve_segment_count() == m - 1);

    for (int i = 0; i < m; i++) {
        y2[i] = (i == 0) ? swrscan_stub_curve_y(0, 0) : swrscan_stub_curve_y(i - 1, 1);
    }

    CHECK(memcmp(y, y2, (size_t) m * sizeof(y[0])) == 0);

    printf("#119 forma dibujada (dataset real de 496 puntos, %d muestras del barrido):\n", points);
    printf("  paso maximo entre puntos dibujados: %d px (la escalera del #104 da 8 px)\n", worst_step);
    printf("  caida maxima a la izquierda/derecha del valle: %d / %d px (escalera: decenas)\n",
           worst_left, worst_right);
    printf("  valle dibujado en la muestra %.2f, minimo medido en la muestra %d (delta %.2f)\n",
           (double) c_min, s_min, (double) delta);
}

/*
 * #125: el barrido TERMINA solo (51 puntos x N pasadas) y el dialogo NO se cierra; el cierre del
 * lado de LVGL (boton a Run, memoria restaurada) lo corre el scheduler. Antes (#119) el barrido
 * envolvia y seguia midiendo para siempre.
 */
void test_swrscan_sweep_end(void) {
    const int32_t span = 50000;

    dialog_quiesce();
    swrscan_stub_set_fg_freq(7028544);
    swrscan_stub_set_span(span);

    dialog_swrscan->construct_cb(NULL);

    button_item_t *run = run_button();

    run->press(run);
    run_one_pass();

    /* Una pasada completa termina la sesion: el TX se apaga, el VFO vuelve al centro de la app y
     * el dialogo queda VIVO (el cierre es del usuario). */
    CHECK(swrscan_stub_destructed() == 0);          /* nadie cerro el dialogo */
    CHECK(run->label != NULL);
    CHECK(strcmp(run->label, "Stop") == 0);         /* el job de cierre todavia no corrio */
    CHECK(swrscan_stub_stopped() == 1);             /* el TX del barrido se apago solo */
    CHECK(swrscan_stub_null_label() == 0);          /* ni lo cerro con el callback en NULL (#85) */
    CHECK(swrscan_stub_pending_jobs() == 1);        /* el cierre LVGL quedo encolado */

    /* El hilo de LVGL corre el job: boton a Run y el barrido queda quieto. */
    swrscan_stub_run_scheduler();
    CHECK(strcmp(run->label, "Run") == 0);
    CHECK(swrscan_stub_destructed() == 0);

    /* La grafica quedo CONGELADA: mas paquetes no mueven el VFO ni reabren la sesion. */
    int before = swrscan_stub_freq_count();

    dialog_swrscan_update(1.5f);
    dialog_swrscan_update(1.5f);

    CHECK(swrscan_stub_freq_count() == before);
    CHECK(swrscan_stub_started() == 1);             /* no arranco una sesion nueva */
    CHECK(strcmp(run->label, "Run") == 0);

    /* Un Run nuevo arranca limpio: vuelve al borde bajo de la grilla. */
    run->press(run);
    CHECK(swrscan_stub_freq(before) == 7028544 - span / 2);
    CHECK(strcmp(run->label, "Stop") == 0);

    run->press(run);                                /* Stop: el cierre lo pide el usuario */
    CHECK(swrscan_stub_stopped() == 2);
}

/*
 * #125: la multipasada (3 = Stable, 5 = Fine) sobre TX real esta GATEADA en el dialogo (el motor
 * SI la soporta, ver test_swr_session_multipass). Con el gate apagado, pedir 3 o 5 pasadas y
 * apretar Run NO transmite: el estado queda en Error con la razon visible y el boton vuelve a Run.
 */
void test_swrscan_multipass_gate(void) {
    CHECK(SWRSCAN_MULTIPASS_TX_ENABLED == 0);       /* el default seguro del dialogo */

    dialog_quiesce();
    swrscan_stub_set_fg_freq(7100000);
    swrscan_stub_set_span(100000);

    dialog_swrscan->construct_cb(NULL);

    button_item_t *run    = run_button();
    button_item_t *passes = passes_button();

    CHECK(strcmp(passes->label_fn(), "Passes:\n1") == 0);
    CHECK(strcmp(swrscan_stub_refreshed_label(), "") == 0);   /* todavia nadie repinto */

    passes->press(passes);                          /* 1 -> 3 (Stable) */
    CHECK(strcmp(passes->label_fn(), "Passes:\n3 *") == 0);   /* "*": TX no habilitado */
    /* Lo que VE el operador: el callback tiene que pedir el repintado (el boton no es un
     * subject, asi que no hay MSG_PARAM_CHANGED que lo refresque). */
    CHECK(strcmp(swrscan_stub_refreshed_label(), "Passes:\n3 *") == 0);

    run->press(run);
    CHECK(swrscan_stub_started() == 0);             /* CERO TX */
    CHECK(strcmp(run->label, "Run") == 0);          /* el boton no quedo en Stop */
    CHECK(swrscan_stub_freq_count() == 0);          /* ni movio el VFO */
    CHECK(strstr(dialog_swrscan_state_text(), "Error") != NULL);
    CHECK(strstr(dialog_swrscan_state_text(), "RF validation") != NULL);

    passes->press(passes);                          /* 3 -> 5 (Fine) */
    CHECK(strcmp(passes->label_fn(), "Passes:\n5 *") == 0);
    CHECK(strcmp(swrscan_stub_refreshed_label(), "Passes:\n5 *") == 0);

    run->press(run);
    CHECK(swrscan_stub_started() == 0);             /* tampoco */
    CHECK(strstr(dialog_swrscan_state_text(), "Error") != NULL);

    /* Volver a 1 pasada (Quick) deja el barrido funcionando otra vez. */
    passes->press(passes);                          /* 5 -> 1 */
    CHECK(strcmp(passes->label_fn(), "Passes:\n1") == 0);

    run->press(run);
    CHECK(swrscan_stub_started() == 1);
    CHECK(strcmp(run->label, "Stop") == 0);
    CHECK(strcmp(dialog_swrscan_state_text(), "Scanning pass 1/1  p 1/51") == 0);

    run->press(run);                                /* Stop */
}

/*
 * #125: el estado visible del barrido (Idle / Scanning pass X/Y / Completed / Stopped / Error) y
 * el readout del minimo. El minimo sale del dato MEDIDO agregado (mediana + MAD), nunca del
 * vertice de la parabola que se dibuja.
 */
void test_swrscan_readout(void) {
    const uint32_t center = 7100000;
    const int32_t  span   = 100000;                 /* grid[25] == center */

    dialog_quiesce();
    swrscan_stub_set_fg_freq((int32_t) center);
    swrscan_stub_set_span(span);

    dialog_swrscan->construct_cb(NULL);

    button_item_t *run = run_button();

    CHECK(strcmp(dialog_swrscan_state_text(), "Idle") == 0);

    run->press(run);

    /* Un valle en el centro de la grilla; el resto alto. */
    for (int k = 0; k < SWR_SESSION_POINTS; k++) {
        dialog_swrscan_update((k == 25) ? 1.1f : 2.5f);
    }

    swrscan_stub_run_scheduler();
    swrscan_stub_draw();

    CHECK(strcmp(dialog_swrscan_state_text(), "Completed") == 0);

    const char *txt = swrscan_stub_last_label_text();

    CHECK(strstr(txt, "min 1.1") != NULL);          /* el valle medido */
    CHECK(strstr(txt, "7.100.000") != NULL);        /* la frecuencia del minimo */
    CHECK(strstr(txt, "n=51/51") != NULL);          /* todos los puntos con muestra */

    /* Stop a mitad de camino: la sesion queda marcada como parcial, con lo medido hasta ahi. */
    dialog_quiesce();
    swrscan_stub_set_fg_freq((int32_t) center);
    swrscan_stub_set_span(span);
    dialog_swrscan->construct_cb(NULL);

    run = run_button();
    run->press(run);

    for (int k = 0; k < 10; k++) {
        dialog_swrscan_update(1.8f);
    }

    /* #133: el Stop del operador tiene que pedir el repintado del chart; si no, la leyenda se
     * queda en "Scanning pass 1/1 p 11/51" aunque el `get` ya diga Stopped (lo mostro el banco). */
    int refreshes = swrscan_stub_chart_refreshes();

    run->press(run);                                /* Stop del operador */

    CHECK(strcmp(dialog_swrscan_state_text(), "Stopped (partial)") == 0);
    CHECK(strcmp(run->label, "Run") == 0);
    CHECK(swrscan_stub_stopped() == 1);
    CHECK(swrscan_stub_chart_refreshes() == refreshes + 1);

    swrscan_stub_draw();
    CHECK(strstr(swrscan_stub_last_label_text(), "n=10/51") != NULL);

    /* Sin datos medidos, el readout lo dice: nunca inventa un minimo. */
    dialog_quiesce();
    swrscan_stub_set_fg_freq((int32_t) center);
    swrscan_stub_set_span(span);
    dialog_swrscan->construct_cb(NULL);

    run = run_button();

    swrscan_stub_draw();
    CHECK(strcmp(dialog_swrscan_state_text(), "Idle") == 0);
    CHECK(strcmp(swrscan_stub_last_label_text(), "No measurements") == 0);
}

/*
 * #125: el watchdog de estancamiento. Si el flow de la radio deja de mandar paquetes, el poll
 * (que corre en el hilo de la radio) aborta la sesion, apaga el TX y encola el cierre LVGL con el
 * estado en Error. La grafica queda congelada con lo medido.
 */
void test_swrscan_timeout(void) {
    const uint32_t center = 7100000;

    dialog_quiesce();
    swrscan_stub_set_fg_freq((int32_t) center);
    swrscan_stub_set_span(100000);
    swrscan_stub_time_set(1000);

    dialog_swrscan->construct_cb(NULL);

    button_item_t *run = run_button();

    run->press(run);

    for (int k = 0; k < 5; k++) {
        dialog_swrscan_update(1.8f);
    }

    /* Sin paquetes, el watchdog de la pasada (SWRSCAN_PASS_TIMEOUT_MS) dispara. */
    swrscan_stub_time_set(1000 + SWRSCAN_PASS_TIMEOUT_MS + 1);

    dialog_swrscan_poll();
    CHECK(swrscan_stub_stopped() == 1);             /* el TX se apago */
    CHECK(swrscan_stub_pending_jobs() == 1);        /* el cierre LVGL quedo encolado */
    CHECK(strstr(dialog_swrscan_state_text(), "Error") != NULL);
    CHECK(strstr(dialog_swrscan_state_text(), "timeout") != NULL);

    swrscan_stub_run_scheduler();
    CHECK(strcmp(run->label, "Run") == 0);
    CHECK(strcmp(dialog_swrscan_state_text(), "Error: RF timeout") == 0);
}

/*
 * #133: Run/Stop del barrido por control remoto.
 *
 * El comando del socket (`{"cmd":"swrscan","val":1|0}`) entra por el scheduler al hilo de
 * LVGL y llama a dialog_swrscan_remote_run(). Lo que se pinnea aca es el CONTRATO de esa
 * funcion, que es donde vive el riesgo (TRANSMITE a 0.1 W):
 *  - solo actua con la ventana del SWR Scan abierta: cerrada (o con otra app abierta)
 *    rechaza sin tocar la radio ni el VFO;
 *  - arranca por el MISMO camino que el boton Run (radio_start_swrscan, sesion #125), y
 *    respeta el gate de multipasada del dialogo;
 *  - es idempotente: un segundo Run con el barrido corriendo no reinicia ni re-transmite;
 *  - Stop funciona DURANTE el barrido (estado RADIO_SWRSCAN, el caso que `btn` no alcanzaba,
 *    hallazgo del #119), apaga el TX y devuelve el VFO al centro;
 *  - cerrar la ventana con el barrido corriendo (#85) y pedir Run despues no crashea ni
 *    transmite.
 * El estado que el QA espera por `{"cmd":"get"}` sale de dialog_swrscan_remote_state().
 */
void test_swrscan_remote(void) {
    const int32_t center = 7100000;
    const int32_t span   = 100000;
    char          state[256];

    dialog_quiesce();
    swrscan_stub_set_fg_freq(center);
    swrscan_stub_set_span(span);
    dialog_swrscan->construct_cb(NULL);

    button_item_t *run    = run_button();
    button_item_t *passes = passes_button();

    CHECK(strcmp(run->label, "Run") == 0);
    CHECK(dialog_swrscan_remote_state(state, sizeof(state)) > 0);
    CHECK(strstr(state, "\"swrscan_state\":\"Idle\"") != NULL);
    CHECK(strstr(state, "\"points\":51") != NULL);      /* la grilla del #125 es fija */
    CHECK(strstr(state, "\"swrscan_tx\":false") != NULL);

    /* --- app cerrada: rechaza sin TX ni movimiento del VFO --- */
    CHECK(dialog_swrscan_remote_run(true) == false);
    CHECK(swrscan_stub_started() == 0);
    CHECK(swrscan_stub_freq_count() == 0);
    CHECK(strcmp(run->label, "Run") == 0);

    /* --- otra app abierta: la ventana del SWR Scan ya no es la actual --- */
    swrscan_stub_dialog_other_open();
    CHECK(dialog_swrscan_remote_run(true) == false);
    CHECK(swrscan_stub_started() == 0);
    CHECK(swrscan_stub_freq_count() == 0);

    /* --- SWRScan abierta: Run remoto arranca la misma sesion que el boton --- */
    swrscan_stub_dialog_open();
    CHECK(dialog_swrscan_remote_run(true) == true);
    CHECK(swrscan_stub_started() == 1);
    CHECK(swrscan_stub_freq_count() == 1);
    CHECK(swrscan_stub_freq(0) == center - span / 2);
    CHECK(strcmp(run->label, "Stop") == 0);
    CHECK(strstr(dialog_swrscan_state_text(), "Scanning") != NULL);

    CHECK(dialog_swrscan_remote_state(state, sizeof(state)) > 0);
    CHECK(strstr(state, "\"swrscan_state\":\"Scanning\"") != NULL);
    CHECK(strstr(state, "\"passes\":1") != NULL);
    CHECK(strstr(state, "\"pass\":1") != NULL);
    CHECK(strstr(state, "\"points\":51") != NULL);
    CHECK(strstr(state, "\"swrscan_tx\":true") != NULL);

    /* --- doble Run: idempotente (no reinicia la sesion ni re-transmite) --- */
    int freqs_before = swrscan_stub_freq_count();

    CHECK(dialog_swrscan_remote_run(true) == true);
    CHECK(swrscan_stub_started() == 1);
    CHECK(swrscan_stub_freq_count() == freqs_before);
    CHECK(strcmp(run->label, "Stop") == 0);

    /* --- Stop remoto CON el barrido transmitiendo: corta y restaura --- */
    for (int k = 0; k < 10; k++) {
        dialog_swrscan_update(1.8f);
    }

    CHECK(radio_get_state() == RADIO_SWRSCAN);      /* el barrido es el que transmite */

    int refreshes = swrscan_stub_chart_refreshes();

    CHECK(dialog_swrscan_remote_run(false) == true);
    CHECK(swrscan_stub_stopped() == 1);
    CHECK(radio_get_state() == RADIO_RX);
    CHECK(strcmp(run->label, "Run") == 0);
    CHECK(strstr(dialog_swrscan_state_text(), "Stopped") != NULL);
    CHECK(swrscan_stub_chart_refreshes() == refreshes + 1);   /* Stop remoto repinta */
    CHECK(swrscan_stub_freq(swrscan_stub_freq_count() - 1) == center);   /* VFO restaurado */

    CHECK(dialog_swrscan_remote_state(state, sizeof(state)) > 0);
    CHECK(strstr(state, "\"swrscan_state\":\"Stopped\"") != NULL);
    CHECK(strstr(state, "\"swrscan_tx\":false") != NULL);

    /* --- Stop sin barrido: no-op con respuesta clara --- */
    int stopped_before = swrscan_stub_stopped();

    CHECK(dialog_swrscan_remote_run(false) == true);
    CHECK(swrscan_stub_stopped() == stopped_before);

    /* --- una sesion completa deja Completed y el TX apagado --- */
    CHECK(dialog_swrscan_remote_run(true) == true);
    CHECK(swrscan_stub_started() == 2);

    for (int k = 0; k < SWR_SESSION_POINTS; k++) {
        dialog_swrscan_update(1.5f);
    }

    CHECK(strstr(dialog_swrscan_state_text(), "Completed") != NULL);
    CHECK(dialog_swrscan_remote_state(state, sizeof(state)) > 0);
    CHECK(strstr(state, "\"swrscan_state\":\"Completed\"") != NULL);
    CHECK(strstr(state, "\"swrscan_tx\":false") != NULL);

    swrscan_stub_run_scheduler();                   /* el cierre LVGL del #125 */
    CHECK(strcmp(run->label, "Run") == 0);
    CHECK(dialog_swrscan_remote_run(false) == true);    /* Stop ya quieto: no-op */

    /* --- equipo transmitiendo por otra causa: NO arranca (no hay TX) --- */
    dialog_quiesce();
    swrscan_stub_set_fg_freq(center);
    swrscan_stub_set_span(span);
    dialog_swrscan->construct_cb(NULL);
    swrscan_stub_dialog_open();

    run = run_button();
    swrscan_stub_set_radio_state(RADIO_TX);

    int started_before = swrscan_stub_started();

    CHECK(dialog_swrscan_remote_run(true) == false);
    CHECK(swrscan_stub_started() == started_before);
    CHECK(swrscan_stub_freq_count() == 0);
    CHECK(strcmp(run->label, "Run") == 0);

    swrscan_stub_set_radio_state(RADIO_RX);

    /* --- el gate de multipasada del dialogo NO se saltea por remoto --- */
    passes->press(passes);                          /* 1 -> 3 (Stable, gateada) */
    CHECK(strcmp(passes->label_fn(), "Passes:\n3 *") == 0);

    started_before = swrscan_stub_started();

    CHECK(dialog_swrscan_remote_run(true) == false);
    CHECK(swrscan_stub_started() == started_before);
    CHECK(swrscan_stub_freq_count() == 0);
    CHECK(strstr(dialog_swrscan_state_text(), "Error") != NULL);
    CHECK(dialog_swrscan_remote_state(state, sizeof(state)) > 0);
    CHECK(strstr(state, "\"swrscan_state\":\"Error\"") != NULL);

    passes->press(passes);                          /* 3 -> 5 */
    passes->press(passes);                          /* 5 -> 1 (Quick) */
    CHECK(strcmp(passes->label_fn(), "Passes:\n1") == 0);

    /* --- cerrar la ventana con el barrido corriendo (#85) y pedir Run despues:
     *     sin crash, sin TX (el dialogo ya no existe para el comando) --- */
    CHECK(dialog_swrscan_remote_run(true) == true);
    CHECK(swrscan_stub_started() == started_before + 1);

    refreshes = swrscan_stub_chart_refreshes();

    dialog_swrscan->destruct_cb();                  /* lo que llama dialog_destruct */
    swrscan_stub_dialog_close();
    CHECK(swrscan_stub_chart_refreshes() == refreshes);   /* cierre: no repinta el chart */

    int started_closed = swrscan_stub_started();
    int freqs_closed   = swrscan_stub_freq_count();

    CHECK(dialog_swrscan_remote_run(true) == false);
    CHECK(dialog_swrscan_remote_run(false) == false);   /* Stop de una ventana muerta: no-op */
    CHECK(swrscan_stub_started() == started_closed);
    CHECK(swrscan_stub_freq_count() == freqs_closed);
}

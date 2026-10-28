/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "dialog_swrscan.h"

#include "dialog.h"
#include "styles.h"
#include "theme.h"
#include "params/params.h"
#include "radio.h"
#include "events.h"
#include "util.h"
#include "keyboard.h"
#include "main_screen.h"
#include "buttons.h"
#include "swr_session.h"
#include "scheduler.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

/* El barrido no puede pasar el presupuesto de duracion de UNA pasada (issue #113): el dwell es
 * un paquete del flow de la radio, no un parametro de la GUI, asi que mas puntos alargan el
 * barrido. La grilla ahora es de 51 puntos fijos (#125), asi que el presupuesto es exactamente
 * 51 x dwell. Subirlo (o habilitar multipasada) es decision del owner. */
_Static_assert(SWR_SESSION_POINTS * SWRSCAN_DWELL_MS == SWRSCAN_TX_BUDGET_MS,
               "el barrido de ROE no coincide con el presupuesto de duracion (SWRSCAN_TX_BUDGET_MS)");

/* La potencia del barrido tiene que estar dentro del control del equipo (issue #113: "la minima
 * posible" = el piso, no menos): por debajo del piso el control no llega, y por encima del techo
 * se sale del rango que declara el equipo (src/vol.c). */
_Static_assert(SWRSCAN_TX_POWER_DW >= SWRSCAN_PWR_FLOOR_DW &&
               SWRSCAN_TX_POWER_DW <= SWRSCAN_PWR_CEILING_DW,
               "la potencia del barrido de ROE esta fuera del control del equipo");

/* La multipasada sobre TX real esta apagada (ver dialog_swrscan.h): el gate del dialogo es
 * 1 pasada. La constante tiene que ser 0 o 1 (no cualquier valor "verdadero"). */
_Static_assert(SWRSCAN_MULTIPASS_TX_ENABLED == 0 || SWRSCAN_MULTIPASS_TX_ENABLED == 1,
               "SWRSCAN_MULTIPASS_TX_ENABLED tiene que ser 0 o 1");

static lv_obj_t             *chart;

/* Agregado por punto (mediana entre pasadas) que se dibuja: NaN = punto sin muestra valida. Se
 * calcula del motor (swr_session) en el hilo de la radio; el chart lo lee al dibujar. */
static float                data_agg[SWR_SESSION_POINTS];

static lv_coord_t           w;
static lv_coord_t           h;

static bool                 run = false;        /* sesion activa (boton en Stop) */
static int                  retries_at_point;   /* paquetes sin potencia en el punto actual */
static bool                 tx_on = false;      /* el TX del barrido esta encendido */

static swr_session_t        session;
static swr_grid_t           grid;
static bool                 grid_ready = false;

static int                  passes_requested = SWR_SESSION_PASSES_QUICK;

static uint32_t             freq_center;

static bool    linear;
static int32_t span;

static ObserverDelayed *freq_obs;
static ObserverDelayed *linear_obs;
static ObserverDelayed *span_obs;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void ui_deleted_cb(lv_event_t * e);
static void key_cb(lv_event_t * e);

static void swrscan_stop(void);
static void swrscan_release(void);
static bool swrscan_begin(void);

static void dialog_swrscan_run_cb(button_item_t *item);
static void dialog_swrscan_scale_cb(button_item_t *item);
static void dialog_swrscan_span_cb(button_item_t *item);
static void dialog_swrscan_passes_cb(button_item_t *item);

static void set_span(Subject *subj, void *user_data);
static void set_linear(Subject *subj, void *user_data);

static const char *scale_label_fn();
static const char *span_label_fn();
static const char *passes_label_fn();

static button_item_t btn_run = {
    .type  = BTN_TEXT,
    .label = "Run",
    .press = dialog_swrscan_run_cb,
};
static button_item_t btn_scale = {
    .type  = BTN_TEXT_FN,
    .label_fn = scale_label_fn,
    .press = dialog_swrscan_scale_cb,
    .subj = &cfg.swrscan_linear.val,
};
static button_item_t btn_span = {
    .type  = BTN_TEXT_FN,
    .label_fn = span_label_fn,
    .press = dialog_swrscan_span_cb,
    .subj = &cfg.swrscan_span.val,
};
static button_item_t btn_passes = {
    .type  = BTN_TEXT_FN,
    .label_fn = passes_label_fn,
    .press = dialog_swrscan_passes_cb,
};

static buttons_page_t btn_page = {
    {
     &btn_run,
     &btn_scale,
     &btn_span,
     &btn_passes,
     }
};

static dialog_t             dialog = {
    .run = false,
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .audio_cb = NULL,
    .btn_page = &btn_page,
    .key_cb = key_cb
};

dialog_t                    *dialog_swrscan = &dialog;

/* Recalcula el agregado por punto (mediana) que dibuja el chart. Corre en el hilo de la radio. */
static void refresh_aggregate(void) {
    int points = grid_ready ? session.grid.points : 0;

    for (int i = 0; i < SWR_SESSION_POINTS; i++) {
        if (i >= points) {
            data_agg[i] = NAN;
            continue;
        }

        swr_point_stat_t st;

        swr_point_stat(&session.points[i], &st);
        data_agg[i] = (st.n_valid > 0) ? st.median : NAN;
    }
}

/* Nueva sesion vacia (abrir el dialogo, cambiar span/frecuencia, re-Run): la grafica queda sin
 * datos y el estado en Idle. Nunca se mezclan lecturas de grillas distintas. */
static void swrscan_reset(void) {
    memset(&session, 0, sizeof(session));
    memset(&grid, 0, sizeof(grid));

    grid_ready       = false;
    session.state    = SWR_SESSION_IDLE;
    session.abort_reason = SWR_ABORT_NONE;
    passes_requested = (passes_requested < 1) ? SWR_SESSION_PASSES_QUICK : passes_requested;

    freq_center = (uint32_t) subject_get_int(cfg_cur.fg_freq);

    refresh_aggregate();
}

/* Arranca una sesion de medicion: grilla de 51 puntos + primera frecuencia + TX. Devuelve false
 * (con el estado del motor en Error y su razon) si la grilla no es legal o la multipasada esta
 * gateada. Corre en el hilo de LVGL (boton Run). */
static bool swrscan_begin(void) {
    uint32_t now = (uint32_t) get_time();

    freq_center = (uint32_t) subject_get_int(cfg_cur.fg_freq);

    swr_grid_result_t gr = swr_grid_build(freq_center, span, SWR_SESSION_POINTS, &grid);

    if (gr != SWR_GRID_OK) {
        grid_ready = false;
        swrscan_reset();
        swr_session_abort(&session, (gr == SWR_GRID_RANGE) ? SWR_ABORT_RANGE : SWR_ABORT_INVALID, now);
        return false;
    }

    grid_ready = true;

    swr_limits_t limits;

    swr_limits_default(&limits);
    limits.max_passes          = SWRSCAN_MULTIPASS_TX_ENABLED ? SWR_SESSION_MAX_PASSES
                                                              : SWR_SESSION_PASSES_QUICK;
    limits.settle_ms           = SWRSCAN_SETTLE_MS;
    /* Las pasadas impares barren de arriba hacia abajo (contrato del backend fake y del fixture
     * del owner: la misma grilla, el mismo indice por frecuencia, direccion alternada). Con UNA
     * pasada no cambia nada: la pasada 0 siempre sube. */
    limits.alternate_direction = true;
    limits.pass_timeout_ms     = SWRSCAN_PASS_TIMEOUT_MS;
    limits.session_timeout_ms  = SWRSCAN_PASS_TIMEOUT_MS * (uint32_t) passes_requested;

    uint32_t    freq = 0;
    retries_at_point = 0;
    swr_action_t act = swr_session_begin(&session, &grid, passes_requested, &limits, now, &freq);

    refresh_aggregate();

    if (act != SWR_ACT_SET_FREQ) {
        /* El motor ya dejo state=Error y abort_reason (gate/rango/pasadas invalidas). */
        return false;
    }

    mem_save(MEM_BACKUP_ID);
    radio_set_freq((int32_t) freq);
    swr_session_tuned(&session, now);   /* la ventana de asentamiento arranca al ordenar el VFO */

    tx_on = radio_start_swrscan();

    if (!tx_on) {
        swr_session_abort(&session, SWR_ABORT_STATE_CHANGED, now);
        return false;
    }

    run = true;

    return true;
}

/* Apaga el TX del barrido. Idempotente y seguro desde el hilo de la radio: solo toma el lock de
 * radio (radio_stop_swrscan). NO toca LVGL ni la memoria. */
static void swrscan_tx_off(void) {
    if (!tx_on) {
        return;
    }

    tx_on = false;
    radio_stop_swrscan();
}

/* Cierre de la sesion del lado de LVGL (lo encola el hilo de la radio por el scheduler cuando el
 * barrido termina solo): boton a Run, restaurar la memoria y liberar el estado. Idempotente y
 * seguro si el dialogo ya se cerro (run queda en false). */
static void swrscan_finished_job(void *arg) {
    (void) arg;

    if (!run) {
        return;
    }

    run = false;
    mem_load(MEM_BACKUP_ID);
    button_update_label(&btn_run, "Run");
}

/* Pide el repintado del chart solo si el objeto sigue vivo. El chart se marca NULL al empezar
 * el cierre del dialogo (swrscan_release), antes de swrscan_stop(): asi el Stop de cierre no
 * redibuja sobre un objeto que LVGL esta por liberar (#85). */
static void swrscan_chart_refresh(void) {
    if (chart) {
        event_send(chart, LV_EVENT_REFRESH, NULL);
    }
}

/* Apaga el barrido y devuelve el estado de radio que armo (radio_stop_swrscan: potencia y modo) y
 * la frecuencia/memoria de la app. Idempotente: la llaman destruct_cb, el LV_EVENT_DELETE y el
 * Stop del operador (hilo de LVGL). */
static void swrscan_stop(void) {
    if (!run && !tx_on) {
        return;
    }

    /* La sesion queda marcada como detenida por el operador: la grafica conserva lo medido y el
     * estado visible lo dice (Stopped (parcial)), en vez de seguir mostrando "Scanning" (#125). */
    swr_session_stop(&session, (uint32_t) get_time());

    run = false;
    swrscan_tx_off();
    radio_set_freq((int32_t) freq_center);
    mem_load(MEM_BACKUP_ID);

    /* El estado logico ya es Stopped: hay que repintar el chart o la leyenda se queda en
     * "Scanning pass 1/1 p N/51" hasta el proximo evento (defecto del Stop, #133). */
    swrscan_chart_refresh();
}

/* Cualquier camino que mate el dialogo pasa por aca: primero se sueltan los observadores (que
 * tocan el chart) y despues se apaga el barrido. Idempotente. */
static void swrscan_release(void) {
    /* El chart se va con el dialogo: marcarlo muerto ANTES de swrscan_stop() hace que el Stop
     * de cierre no pida un repintado sobre un objeto en destruccion (#85). */
    chart = NULL;

    if (freq_obs) {
        observer_delayed_del(freq_obs);
        freq_obs = NULL;
    }
    if (linear_obs) {
        observer_delayed_del(linear_obs);
        linear_obs = NULL;
    }
    if (span_obs) {
        observer_delayed_del(span_obs);
        span_obs = NULL;
    }

    swrscan_stop();
}

static lv_coord_t calc_y(float vswr) {
    float x;

    if (linear) {
        x = (vswr - 1.0f) / (5.0f - 1.0f);
    } else {
        float c = 1.0f / logf(10.0);

        x = log10f(1.0f + (vswr - 1.0f) / c);
    }

    return (1.0f - x) * h;
}

/* Texto corto del motivo de abort/error, para el readout. */
static const char *abort_text(swr_abort_reason_t reason) {
    switch (reason) {
        case SWR_ABORT_USER:            return "Stop";
        case SWR_ABORT_TIMEOUT:         return "RF timeout";
        case SWR_ABORT_NO_DATA:         return "no data";
        case SWR_ABORT_NO_TX:           return "no TX";
        case SWR_ABORT_STATE_CHANGED:   return "state change";
        case SWR_ABORT_MULTIPASS_GATED: return "3/5 passes require RF validation";
        case SWR_ABORT_RANGE:           return "span/center outside the TX band";
        case SWR_ABORT_INVALID:         return "invalid parameters";
        case SWR_ABORT_NONE:
        default:                        return "";
    }
}

/* El estado visible (issue #125): Idle / Scanning pass X/Y / Completed / Stopped / Error. */
const char *dialog_swrscan_state_text(void) {
    static char buf[64];

    switch (session.state) {
        case SWR_SESSION_SCANNING:
            snprintf(buf, sizeof(buf), "Scanning pass %d/%d  p %d/%d",
                     session.pass + 1, session.passes_requested, session.point_seq + 1, session.grid.points);
            break;

        case SWR_SESSION_COMPLETED:
            snprintf(buf, sizeof(buf), "Completed");
            break;

        case SWR_SESSION_STOPPED:
            snprintf(buf, sizeof(buf), "Stopped (partial)");
            break;

        case SWR_SESSION_ERROR:
            snprintf(buf, sizeof(buf), "Error: %s", abort_text(session.abort_reason));
            break;

        case SWR_SESSION_IDLE:
        default:
            snprintf(buf, sizeof(buf), "Idle");
            break;
    }

    return buf;
}

/* El minimo y su dispersion salen SIEMPRE del dato medido agregado, nunca del vertice del
 * ajuste que se dibuja (issue #125). */
static void format_readout(char *buf, size_t n) {
    int   point;
    float swr;
    float mad;

    if (!swr_session_minimum(&session, &point, &swr, &mad)) {
        snprintf(buf, n, "No measurements");
        return;
    }

    uint16_t mhz, khz, hz;

    split_freq((int32_t) session.points[point].freq_hz, &mhz, &khz, &hz);

    int valid = swr_session_valid_points(&session);

    if (isnan(mad)) {
        snprintf(buf, n, "min %.1f @ %u.%03u.%03u kHz (n=%d/%d)",
                 (double) swr, mhz, khz, hz, valid, session.grid.points);
    } else {
        snprintf(buf, n, "min %.1f +/- %.1f @ %u.%03u.%03u kHz (n=%d/%d)",
                 (double) swr, (double) mad, mhz, khz, hz, valid, session.grid.points);
    }
}

/* La curva ES el dato: polilinea por los puntos medidos (agregado por frecuencia) y un punto
 * por muestra. Un punto sin medicion corta el tramo (hueco): no se interpola ni se ajusta una
 * parabola decorativa. La forma la manda la medicion, no una estetica. */
static void draw_curve(lv_draw_ctx_t *ctx, const lv_draw_line_dsc_t *line_dsc,
                       lv_coord_t x1, lv_coord_t y1) {
    int points = grid_ready ? session.grid.points : 0;

    if (points < 2) {
        return;
    }

    lv_draw_rect_dsc_t dot_dsc;

    lv_draw_rect_dsc_init(&dot_dsc);
    dot_dsc.bg_color = line_dsc->color;
    dot_dsc.bg_opa   = LV_OPA_COVER;

    bool have_prev = false;
    lv_coord_t prev_x = 0;
    lv_coord_t prev_y = 0;

    for (int i = 0; i < points; i++) {
        if (isnan(data_agg[i])) {
            have_prev = false;
            continue;
        }

        lv_coord_t x = x1 + (lv_coord_t) ((int32_t) i * w / (points - 1));
        lv_coord_t y = y1 + calc_y(data_agg[i]);

        if (have_prev) {
            lv_point_t a = { prev_x, prev_y };
            lv_point_t b = { x, y };

            lv_draw_line(ctx, line_dsc, &a, &b);
        }

        lv_area_t dot = { x - 1, y - 1, x + 1, y + 1 };

        lv_draw_rect(ctx, &dot_dsc, &dot);

        prev_x    = x;
        prev_y    = y;
        have_prev = true;
    }
}

/* Marca la frecuencia de la ROE minima MEDIDA (agregada): un tick vertical en el valle. */
static void draw_min_marker(lv_draw_ctx_t *ctx, lv_coord_t x1, lv_coord_t y1) {
    int   point;
    float swr;
    float mad;

    if (!grid_ready || !swr_session_minimum(&session, &point, &swr, &mad)) {
        return;
    }

    lv_draw_line_dsc_t dsc;

    lv_draw_line_dsc_init(&dsc);
    dsc.color = TC(text);
    dsc.width = 2;

    lv_point_t a, b;

    a.x = b.x = x1 + (lv_coord_t) ((int32_t) point * w / (session.grid.points - 1));
    a.y = y1 + calc_y(swr) - 8;
    b.y = y1 + calc_y(swr) + 8;

    lv_draw_line(ctx, &dsc, &a, &b);
}

static void draw_cb(lv_event_t * e) {
    lv_obj_t            *obj = lv_event_get_target(e);
    lv_draw_ctx_t       *draw_ctx = lv_event_get_draw_ctx(e);
    lv_draw_line_dsc_t  line_dsc;
    char                str[64];

    lv_draw_line_dsc_init(&line_dsc);

    lv_coord_t x1 = obj->coords.x1;
    lv_coord_t y1 = obj->coords.y1 - 5;

    lv_coord_t w = lv_obj_get_width(obj);
    lv_coord_t h = lv_obj_get_height(obj) - 5;

    lv_point_t a, b;

    /* Grid */

    lv_area_t   area;

    lv_draw_label_dsc_t dsc_label;
    lv_draw_label_dsc_init(&dsc_label);

    /* El texto del chart sale del rol del tema: el chart ahora pinta su propio panel (issue
     * #119) y con el color fijo blanco el eje quedaba ilegible en los temas claros (Sunlit tiene
     * panel y dialogo blancos). */
    dsc_label.color = TC(text);
    dsc_label.font = &sony_28;

    lv_point_t label_size;

    line_dsc.color = lv_color_hex(0xAAAAAA);
    line_dsc.width = 2;

    a.x = x1;
    b.x = x1 + w;

    for (float y = 1.5f; y <= 4.5f; y+= 0.5f) {
        a.y = y1 + calc_y(y);
        b.y = a.y;

        lv_draw_line(draw_ctx, &line_dsc, &a, &b);

        snprintf(str, sizeof(str), "%.1f", y);
        lv_txt_get_size(&label_size, str, dsc_label.font, 0, 0, LV_COORD_MAX, 0);

        area.x1 = a.x;
        area.y1 = a.y - label_size.y / 2;
        area.x2 = a.x + label_size.x;
        area.y2 = a.y + label_size.y / 2;

        lv_draw_label(draw_ctx, &dsc_label, &area, str, NULL);
    }

    uint32_t    freq = freq_center - span / 4;
    uint16_t    mhz, khz, hz;

    a.y = y1;
    b.y = y1 + h;

    for (int16_t x = -1; x <= 1; x++, freq += span / 4) {
        a.x = x1 + w / 2 + (w / 4) * x;
        b.x = a.x;

        lv_draw_line(draw_ctx, &line_dsc, &a, &b);

        split_freq(freq, &mhz, &khz, &hz);
        snprintf(str, sizeof(str), "%i.%03i.%03i", mhz, khz, hz);
        lv_txt_get_size(&label_size, str, dsc_label.font, 0, 0, LV_COORD_MAX, 0);

        area.x1 = a.x - label_size.x / 2;
        area.y1 = y1 + label_size.y / 2;
        area.x2 = area.x1 + label_size.x;
        area.y2 = area.y1 + label_size.y;

        lv_draw_label(draw_ctx, &dsc_label, &area, str, NULL);
    }

    /* Curva: el ajuste cuadratico del agregado medido (decorativo). El minimo que lee el
     * operador sale de las muestras (draw_min_marker + readout), no del vertice del ajuste. */

    line_dsc.color = TC(text);
    line_dsc.width = 4;

    draw_curve(draw_ctx, &line_dsc, x1, y1);
    draw_min_marker(draw_ctx, x1, y1);

    /* Estado y readout (issue #125), sobre el chart. Llevan fondo del propio panel
     * (TC(panel), el mismo rol con el que el chart pinta su fondo desde #119) porque el
     * texto cae encima de la grilla, de las etiquetas del eje Y y de la curva: sin fondo
     * quedaba ilegible (lo mostro la primera captura del arnes). El fondo es
     * semitransparente a proposito: atenua lo que hay debajo, no lo tapa. */
    char status[64];
    char readout[64];

    snprintf(status, sizeof(status), "%s", dialog_swrscan_state_text());
    format_readout(readout, sizeof(readout));

    lv_point_t size_status, size_readout;

    lv_txt_get_size(&size_status, status, dsc_label.font, 0, 0, LV_COORD_MAX, 0);
    lv_txt_get_size(&size_readout, readout, dsc_label.font, 0, 0, LV_COORD_MAX, 0);

    lv_coord_t box_w = ((size_status.x > size_readout.x) ? size_status.x : size_readout.x) + 16;

    lv_draw_rect_dsc_t rect_dsc;

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = TC(panel);
    rect_dsc.bg_opa   = LV_OPA_80;

    area.x1 = x1 + 4;
    area.y1 = y1 + 2;
    area.x2 = x1 + 4 + box_w;
    area.y2 = y1 + 68;
    lv_draw_rect(draw_ctx, &rect_dsc, &area);

    area.x1 = x1 + 12;
    area.x2 = x1 + 4 + box_w - 8;
    area.y1 = y1 + 4;
    area.y2 = area.y1 + 30;
    lv_draw_label(draw_ctx, &dsc_label, &area, status, NULL);

    area.y1 = y1 + 36;
    area.y2 = area.y1 + 30;
    lv_draw_label(draw_ctx, &dsc_label, &area, readout, NULL);
}

static void freq_update_cb(Subject *subj, void *user_data) {
    if (run) {
        return;
    }

    swrscan_reset();
    lv_obj_invalidate(chart);
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    /* El marco de dialogos llama a destruct_cb, pero no siempre (patron de dialog_cw_trainer,
     * HALLAZGOS 37): si el objeto muere por otro camino, el barrido seguia corriendo con el
     * chart ya liberado y el proximo tick de la radio lo desreferenciaba. swrscan_release() es
     * idempotente, asi que el primer camino que llegue apaga todo. */
    lv_obj_add_event_cb(dialog.obj, ui_deleted_cb, LV_EVENT_DELETE, NULL);

    linear_obs = subject_add_delayed_observer_and_call(cfg.swrscan_linear.val, set_linear, NULL);
    span_obs = subject_add_delayed_observer_and_call(cfg.swrscan_span.val, set_span, NULL);

    buttons_unload_page();
    buttons_load_page(&btn_page);

    freq_obs = subject_add_delayed_observer(cfg_cur.fg_freq, freq_update_cb, NULL);

    chart  = lv_obj_create(dialog.obj);

    w = 780;
    h = 330;

    lv_obj_add_event_cb(chart, draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_set_size(chart, w, h);
    lv_obj_center(chart);

    /* El chart pinta su propio fondo (issue #119): el panel del dialogo es una imagen
     * semitransparente (alpha 204/255 en images/dialog.bin) y el chart era el unico contenido
     * sin fondo propio, asi que la pantalla de atras se veia a traves del grafico (el fantasma
     * medido en la radio: la fila de frecuencia y la tira de bandas de la home dentro del
     * chart, docs/HALLAZGOS.md 47). Mismo idioma que las tablas de contenido de los otros
     * dialogos y que el cuerpo de las apps (app_frame_body): el color sale del rol del tema,
     * nunca escrito a mano. */
    lv_obj_set_style_bg_color(chart, TC(panel), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);

    lv_group_add_obj(keyboard_group, chart);
    lv_obj_add_event_cb(chart, key_cb, LV_EVENT_KEY, NULL);

    swrscan_reset();
}

static void destruct_cb(void) {
    swrscan_release();
    radio_set_freq(subject_get_int(cfg_cur.fg_freq));
}

static void ui_deleted_cb(lv_event_t * e) {
    (void) e;

    swrscan_release();
}

static void key_cb(lv_event_t * e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct(&dialog);
            break;

        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;

        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
    }
}

void dialog_swrscan_run_cb(button_item_t *item) {
    if (run) {
        /* item es NULL cuando el cierre apaga el barrido: sin la guarda, button_update_label()
         * desreferencia NULL+4 y la GUI muere (issue #85, fallo=0x4). */
        if (item) {
            button_update_label(item, "Run");
        }
        swrscan_stop();
        return;
    }

    if (!swrscan_begin()) {
        /* El motor dejo el motivo en Error (gate multipasada, rango o parametros). */
        if (item) {
            button_update_label(item, "Run");
        }
        swrscan_chart_refresh();
        return;
    }

    if (item) {
        button_update_label(item, "Stop");
    }
    swrscan_chart_refresh();
}

void dialog_swrscan_scale_cb(button_item_t *item) {
    bool new_val = !subject_get_int(cfg.swrscan_linear.val);
    subject_set_int(cfg.swrscan_linear.val, new_val);
}

void dialog_swrscan_span_cb(button_item_t *item) {
    if (run) {
        return;
    }

    int32_t span = subject_get_int(cfg.swrscan_span.val);

    switch (span) {
        case 50000:
            span = 100000;
            break;

        case 100000:
            span = 200000;
            break;

        case 200000:
            span = 500000;
            break;

        case 500000:
            span = 50000;
            break;
    }
    subject_set_int(cfg.swrscan_span.val, span);

    swrscan_reset();
    swrscan_chart_refresh();
}

/* Multipasada: cicla 1 (Quick) / 3 (Stable) / 5 (Fine). 3 y 5 quedan marcadas "*" porque su TX
 * no esta habilitado (requieren validacion en hardware): el Run las rechaza con el estado
 * Error. Solo se puede cambiar con el barrido detenido. */
void dialog_swrscan_passes_cb(button_item_t *item) {
    if (run) {
        return;
    }

    switch (passes_requested) {
        case SWR_SESSION_PASSES_QUICK:  passes_requested = SWR_SESSION_PASSES_STABLE; break;
        case SWR_SESSION_PASSES_STABLE: passes_requested = SWR_SESSION_PASSES_FINE; break;
        case SWR_SESSION_PASSES_FINE:
        default:                        passes_requested = SWR_SESSION_PASSES_QUICK; break;
    }

    /* El boton TIENE que repintar: `passes_requested` no es un subject, asi que no hay
     * MSG_PARAM_CHANGED que refresque la etiqueta (a diferencia de span/scale). Sin esto el
     * operador aprieta Passes, la sesion corre 5 pasadas y el boton sigue diciendo 1. */
    buttons_refresh(item);
}

void set_span(Subject *subj, void *user_data) {
    span = subject_get_int(subj);
}

void set_linear(Subject *subj, void *user_data) {
    linear = subject_get_int(subj);
}

const char *scale_label_fn() {
    if (subject_get_int(cfg.swrscan_linear.val)) {
        return "Scale:\nLinear";
    } else {
        return "Scale:\nLog";
    }
}

const char *span_label_fn() {
    static char buf[20];
    const char * fmt = "Span:\n%u kHz";
    int32_t val = subject_get_int(cfg.swrscan_span.val);
    sprintf(buf, fmt, val / 1000);
    return buf;
}

/* "*" = multipasada con TX todavia no habilitado (requiere validacion en hardware). */
const char *passes_label_fn() {
    static char buf[24];
    const char * fmt = "Passes:\n%u%s";
    sprintf(buf, fmt, (unsigned) passes_requested,
            (passes_requested > 1) ? " *" : "");
    return buf;
}

/* Acciones comunes a los dos caminos (paquete valido / punto sin medicion). */
static void swrscan_apply_action(swr_action_t act, uint32_t freq) {
    switch (act) {
        case SWR_ACT_SET_FREQ:
            refresh_aggregate();
            radio_set_freq((int32_t) freq);
            swr_session_tuned(&session, (uint32_t) get_time());
            retries_at_point = 0;
            swrscan_chart_refresh();
            break;

        case SWR_ACT_COMPLETE:
        case SWR_ACT_ABORT:
            refresh_aggregate();
            swrscan_tx_off();
            radio_set_freq((int32_t) freq_center);
            scheduler_put_noargs(swrscan_finished_job);
            swrscan_chart_refresh();
            break;

        case SWR_ACT_NONE:
        default:
            break;
    }
}

/* Camino real: radio.c entrega cada paquete del flow con su flag de TX y la potencia reportada.
 * Un paquete sin potencia no es una medicion (el detector reporta ROE 1.0): se reintenta el mismo
 * punto hasta SWRSCAN_RETRY_PACKETS paquetes y, si no aparece una muestra con potencia, el punto
 * queda como hueco (NaN) en vez de inventar 1.0. */
void dialog_swrscan_update_pack(float vswr, float tx_power_w, bool tx_on) {
    if (!run) {
        return;
    }

    int tx_dw = (int) (tx_power_w * 10.0f + 0.5f);

    if (!tx_on || tx_dw < SWRSCAN_MIN_TX_POWER_DW) {
        retries_at_point++;

        if (retries_at_point < SWRSCAN_RETRY_PACKETS) {
            return;   /* mismo punto: esperar un paquete con potencia */
        }

        uint32_t     freq = 0;
        swr_action_t act  = swr_session_skip_point(&session, (uint32_t) get_time(), &freq);

        retries_at_point = 0;
        swrscan_apply_action(act, freq);
        return;
    }

    retries_at_point = 0;

    uint32_t     freq = 0;
    swr_action_t act  = swr_session_packet(&session, vswr, (uint32_t) get_time(), &freq);

    swrscan_apply_action(act, freq);
}

/* Compatibilidad (tests nativos y stubs): un paquete se trata como valido. */
void dialog_swrscan_update(float vswr) {
    dialog_swrscan_update_pack(vswr, 0.9f, true);
}



void dialog_swrscan_poll(void) {
    if (!run) {
        return;
    }

    if (swr_session_poll(&session, (uint32_t) get_time()) == SWR_ACT_ABORT) {
        swrscan_tx_off();
        radio_set_freq((int32_t) freq_center);
        scheduler_put_noargs(swrscan_finished_job);
        swrscan_chart_refresh();
    }
}

/* --- control remoto (issue #133) -------------------------------------------- */

/* Token estable para el QA (no el texto largo del readout): Idle / Scanning / Completed /
 * Stopped / Error. El detalle humano sigue en dialog_swrscan_state_text(). */
static const char *remote_state_name(swr_session_state_t state) {
    switch (state) {
        case SWR_SESSION_SCANNING:  return "Scanning";
        case SWR_SESSION_COMPLETED: return "Completed";
        case SWR_SESSION_STOPPED:   return "Stopped";
        case SWR_SESSION_ERROR:     return "Error";

        case SWR_SESSION_IDLE:
        default:                    return "Idle";
    }
}

bool dialog_swrscan_remote_run(bool start) {
    /* Guarda dura 1: el comando SOLO existe con la ventana del SWR Scan abierta. El servidor
     * remoto ya lo verifica (app_open), pero entre el pedido y esta ejecucion (scheduler del
     * hilo de LVGL) la app pudo cerrarse: sin ventana no hay TX ni desreferencia del chart. */
    if (!dialog_type_is_run(&dialog)) {
        return false;
    }

    /* Idempotencia: Run con el barrido corriendo (o Stop con el barrido quieto) es un no-op
     * con respuesta clara; NO reinicia la sesion ni vuelve a transmitir. Esta comparacion va
     * ANTES del chequeo de estado de radio porque durante el barrido el equipo esta en
     * RADIO_SWRSCAN (no en RX): un segundo Run tiene que ser un no-op, no un rechazo. */
    if (start == run) {
        return true;
    }

    /* Guarda dura 2: arrancar exige el equipo en RX. radio_start_swrscan() tambien lo exige,
     * pero el pedido remoto no puede ni intentarlo sobre una TX ajena (el barrido fuerza LSB
     * y 0.1 W). Stop no necesita esta guarda: tiene que funcionar transmitiendo (#119). */
    if (start && radio_get_state() != RADIO_RX) {
        return false;
    }

    /* Mismo camino que el boton (Run/Stop): no se duplica la logica de potencia, modo,
     * restore (#94/#102) ni la sesion multipasada (#125). */
    dialog_swrscan_run_cb(&btn_run);

    return start == run;
}

void dialog_swrscan_remote(const char *cmd, int32_t val, const char *text) {
    (void) text;

    if (!strcmp(cmd, "swrscan")) {
        dialog_swrscan_remote_run(val != 0);
    }
}

int dialog_swrscan_remote_state(char *buf, size_t size) {
    /* La grilla es de 51 puntos fijos (#125) tambien antes del primer Run: el QA no tiene que
     * adivinar el tamano de la sesion. */
    int points = grid_ready ? session.grid.points : SWR_SESSION_POINTS;
    int passes = grid_ready ? session.passes_requested : passes_requested;
    int pass   = (session.state == SWR_SESSION_SCANNING) ? session.pass + 1 : 0;

    return snprintf(buf, size,
                    "{\"swrscan_state\":\"%s\",\"passes\":%d,\"pass\":%d,\"points\":%d,\"swrscan_tx\":%s}",
                    remote_state_name(session.state), passes, pass, points, tx_on ? "true" : "false");
}

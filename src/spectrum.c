/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "spectrum.h"
#include "theme.h"

#include "dsp.h"
#include "events.h"
#include "meter.h"
#include "params/params.h"
#include "pubsub_ids.h"
#include "radio.h"
#include "recorder.h"
#include "rtty.h"
#include "scheduler.h"
#include "styles.h"
#include "util.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_MIN S4
#define DEFAULT_MAX S9_20
#define VISOR_HEIGHT_TX (100 - 61)
#define VISOR_HEIGHT_RX 100
#define SPECTRUM_SIZE 800

/* Inset vertical de la banda pasante y del texto de BW respecto del borde
 * superior del visor (y1 + h - visor_height = 60 con el espectro de 160 px y el
 * visor RX de 100 px). El S-meter tiene fondo transparente y su region llega a
 * y=66, asi que el relleno/bordes del filtro arrancan en y=66 para no tocar
 * pixeles del medidor ni de la barra superior (contrato #173, PR 1); el texto
 * del BW va unos px mas abajo todavia. */
#define FILTER_BAND_TOP_PAD   6
#define FILTER_BW_TEXT_TOP_PAD (FILTER_BAND_TOP_PAD + 4)
#define FILTER_BW_TEXT_MARGIN  4

typedef struct {
    float    val;
    uint64_t time;
} peak_t;

/* Frame del espectro: el estado que el hilo de radio publica y el hilo de GUI
 * dibuja. Lo escribe el productor (spectrum_data, spectrum_clear y los
 * observers de config) SIEMPRE bajo data_mux; el callback de dibujo lo copia
 * entero a `snapshot` de una sola vez y dibuja de ahi. Asi el lector nunca ve
 * un frame a medio escribir ni usa un puntero al buffer de escritura despues de
 * soltar el mutex, y el mutex no se sostiene durante los 800 lv_draw_line. */
typedef struct {
    float        buf[SPECTRUM_SIZE];
    peak_t       peak[SPECTRUM_SIZE];
    float        grid_min;
    float        grid_max;
    int32_t      width_hz;
    int32_t      filter_from;
    int32_t      filter_to;
    int32_t      filter_bw;
    int32_t      dnf_mode;
    int32_t      dnf_center;
    int32_t      dnf_width;
    bool         tx;
    x6200_mode_t mode;
} spectrum_frame_t;

/* `frame` es el compartido (productor -> lector, bajo data_mux); `snapshot` es
 * la copia privada del hilo de GUI (nadie mas la toca, no lleva lock). */
static spectrum_frame_t frame = {
    .grid_min  = DEFAULT_MIN,
    .grid_max  = DEFAULT_MAX,
    .width_hz  = 100000,
    .filter_to = 3000,
    .filter_bw = 3000,
};
static spectrum_frame_t snapshot;

static lv_obj_t *obj;

/* visor_height lo escriben los eventos TX/RX y lo lee el callback de dibujo, los
 * dos en el hilo de LVGL (event_send encola y event_obj_check drena ahi): no
 * cruza hilos y no necesita el mutex. */
static int16_t visor_height = 100;

static int32_t cur_freq;
static int16_t freq_mod;

/* Inicializador estatico: el estado compartido queda protegido desde el arranque
 * del proceso, sin depender del orden de spectrum_init(). */
static pthread_mutex_t data_mux = PTHREAD_MUTEX_INITIALIZER;

/* Ticket del refresco (bajo data_mux): a lo sumo un spectrum_refresh encolado a
 * la vez. Si la GUI se atrasa, el productor no encola cuadros historicos (el
 * proximo refresco dibuja el frame mas nuevo) y nunca se bloquea. */
static bool refresh_queued;

/* Cadencia del redibujo (perfil ECO/CRITICAL, UI-08): el espectro dibuja 1 de
 * cada `refresh_period` refrescos. Los datos siguen llegando y actualizandose en
 * `frame`; lo que se saltea es la invalidacion completa del widget (el
 * redibujo de LVGL). Es el mismo mecanismo del modo Ahorro del waterfall
 * (src/waterfall.c), con el borde de bajada gratis: al volver el perfil a
 * QUALITY/BALANCED el periodo vuelve a 1 y el proximo refresco dibuja, asi que
 * ningun frame queda en deuda. */
static uint8_t refresh_period = 1;
static uint8_t refresh_counter = 0;

static void on_fft_width_changed(Subject *subj, void *user_data);
static void on_real_filter_from_change(Subject *subj, void *user_data);
static void on_real_filter_to_change(Subject *subj, void *user_data);
static void on_filter_bw_change(Subject *subj, void *user_data);
static void on_cur_mode_change(Subject *subj, void *user_data);
static void on_grid_min_change(Subject *subj, void *user_data);
static void on_grid_max_change(Subject *subj, void *user_data);
static void on_int32_val_change(Subject *subj, void *user_data);
static void on_cur_freq_change(Subject *subj, void *user_data);
static void spectrum_refresh(void *data);

/* Formato compacto del BW real para el texto del espectro: "3.3k" cuando el
 * valor es un multiplo exacto de 100 Hz >= 1000 (sin perder un solo Hz, el caso
 * del golden: 3300 -> "3.3k"), y el entero exacto en Hz en cualquier otro caso
 * (2990 -> "2990"). Nunca redondea el valor mostrado. */
static void format_filter_bw(char *buf, size_t n, int32_t bw) {
    if (bw >= 1000 && bw % 100 == 0) {
        snprintf(buf, n, "%d.%dk", bw / 1000, (bw % 1000) / 100);
    } else {
        snprintf(buf, n, "%d", bw);
    }
}

static void spectrum_draw_cb(lv_event_t *e) {
    lv_obj_t          *obj      = lv_event_get_target(e);
    lv_draw_ctx_t     *draw_ctx = lv_event_get_draw_ctx(e);
    lv_draw_line_dsc_t main_line_dsc;
    lv_draw_line_dsc_t peak_line_dsc;

    /* Snapshot: una copia completa y consistente del frame (800 muestras + 800
     * picos + los metadatos que el dibujo usa) bajo un mutex breve. Todo el
     * dibujo de abajo queda fuera de la seccion critica. */
    pthread_mutex_lock(&data_mux);
    snapshot = frame;
    pthread_mutex_unlock(&data_mux);

    const spectrum_frame_t *f = &snapshot;

    float min, max;

    if (f->tx) {
        min = DEFAULT_MIN;
        max = DEFAULT_MAX;
    } else {
        min = f->grid_min;
        max = f->grid_max;
    }

    /* Factor de escala del eje TX (el ancho de FFT sobre el ancho del canal):
     * se calcula siempre, aunque el frame no sea TX, para que el lazo no lea un
     * valor sin inicializar. */
    uint8_t tx_scale_factor = f->width_hz / 48000;

    /* Lines */

    lv_draw_line_dsc_init(&main_line_dsc);

    main_line_dsc.color = lv_color_hex(theme_hex(TC_text_dim));
    main_line_dsc.width = 1;

    lv_draw_line_dsc_init(&peak_line_dsc);

    peak_line_dsc.color = lv_color_hex(theme_hex(TC_text_mute));
    peak_line_dsc.width = 1;

    lv_coord_t x1 = obj->coords.x1;
    lv_coord_t y1 = obj->coords.y1;

    lv_coord_t w = lv_obj_get_width(obj);
    lv_coord_t h = lv_obj_get_height(obj);

    lv_point_t main_a, main_b;
    lv_point_t peak_a, peak_b;

    if (!params.spectrum_filled) {
        main_b.x = x1;
        main_b.y = y1 + h;
    }

    peak_b.x = x1;
    peak_b.y = y1 + h;

    for (uint16_t i = 0; i < SPECTRUM_SIZE; i++) {
        float v;
        if (f->tx) {
            int16_t sp_ind = ((int16_t)i - SPECTRUM_SIZE / 2) * tx_scale_factor + SPECTRUM_SIZE / 2;
            if ((sp_ind < 0) || (sp_ind > (SPECTRUM_SIZE - 1))) {
                v = S_MIN;
            } else {
                v = f->buf[sp_ind];
            }
        } else {
            v = f->buf[i];
        }
        v = LV_MAX(v - min, 0) / (max - min);
        uint16_t x = i * w / SPECTRUM_SIZE;

        /* Peak */

        if (params.spectrum_peak && !f->tx) {
            float v_peak = f->peak[i].val;
            v_peak = LV_MAX(v_peak - min, 0) / (max - min);
            peak_a.x = x1 + x;
            peak_a.y = y1 + (1.0f - v_peak) * h;

            lv_draw_line(draw_ctx, &peak_line_dsc, &peak_a, &peak_b);

            peak_b = peak_a;
        }

        /* Main */

        main_a.x = x1 + x;
        main_a.y = y1 + (1.0f - v) * h;

        if (params.spectrum_filled) {
            main_b.x = main_a.x;
            main_b.y = y1 + h;
        }

        lv_draw_line(draw_ctx, &main_line_dsc, &main_a, &main_b);

        if (!params.spectrum_filled) {
            main_b = main_a;
        }
    }

    /* Banda pasante RX a escala (issue #173, PR 1).
     *
     * La posicion sale EXCLUSIVAMENTE de los bordes reales de RF que publica
     * cfg/mode.c::update_real_filters() (f->filter_from/to) y del ancho mostrado
     * (f->width_hz), con spectrum_filter_band(): el centro del VFO es w/2 y cada
     * borde cae en w/2 + round(edge * w / span). NO se centra artificialmente una
     * ventana de BW sobre el VFO. El area es semitransparente y discreta,
     * delimitada por dos lineas verticales finas (1 px) en el rol de tema
     * `vfo_line` (el rol que el tema define para "filter band on spectrum /
     * waterfall"); el exterior al filtro no se oscurece. La marca central del VFO
     * se dibuja despues y queda visible. */

    lv_draw_rect_dsc_t rect_dsc;
    lv_area_t          area;

    lv_draw_rect_dsc_init(&rect_dsc);

    int32_t w_hz = f->width_hz;

    int16_t sign_from = (f->filter_from > 0) ? 1 : -1;
    int16_t sign_to   = (f->filter_to > 0) ? 1 : -1;

    int32_t f1, f2;

    spectrum_filter_band_t band = spectrum_filter_band(f->filter_from, f->filter_to, w_hz, w);

    lv_draw_line_dsc_t band_line_dsc;

    lv_draw_line_dsc_init(&band_line_dsc);
    band_line_dsc.color = lv_color_hex(theme_hex(TC_vfo_line));
    band_line_dsc.width = 1;

    if (band.visible) {
        lv_coord_t band_y1 = y1 + h - visor_height + FILTER_BAND_TOP_PAD;
        lv_coord_t band_y2 = y1 + h;

        /* Relleno discreto: mismo rol del tema que los bordes, opacidad baja. */
        rect_dsc.bg_color = lv_color_hex(theme_hex(TC_vfo_line));
        rect_dsc.bg_opa   = LV_OPA_20;

        area.x1 = x1 + band.left;
        area.y1 = band_y1;
        area.x2 = x1 + band.right;
        area.y2 = band_y2;

        lv_draw_rect(draw_ctx, &rect_dsc, &area);

        /* Dos lineas verticales finas en los bordes reales del filtro. */
        main_a.x = x1 + band.left;
        main_a.y = band_y1;
        main_b.x = main_a.x;
        main_b.y = band_y2;
        lv_draw_line(draw_ctx, &band_line_dsc, &main_a, &main_b);

        main_a.x = x1 + band.right;
        main_b.x = main_a.x;
        lv_draw_line(draw_ctx, &band_line_dsc, &main_a, &main_b);

        /* Texto chico con el BW real, SOLO si entra entre las dos lineas sin
         * tapar el medidor ni la franja IARU; si no entra, se omite. Jamas se
         * agranda el filtro para que entre. */
        if (f->filter_bw > 0 && (band.right - band.left) > 2 * FILTER_BW_TEXT_MARGIN) {
            char                txt[16];
            lv_point_t          txt_size;
            lv_draw_label_dsc_t label_dsc;

            format_filter_bw(txt, sizeof(txt), f->filter_bw);

            lv_draw_label_dsc_init(&label_dsc);
            label_dsc.color = lv_color_hex(theme_hex(TC_vfo_line));
            label_dsc.font  = &sony_14;
            label_dsc.align = LV_TEXT_ALIGN_CENTER;

            lv_txt_get_size(&txt_size, txt, label_dsc.font, 0, 0, LV_COORD_MAX, 0);

            if (txt_size.x <= (band.right - band.left) - 2 * FILTER_BW_TEXT_MARGIN) {
                lv_coord_t ty = band_y1 + FILTER_BW_TEXT_TOP_PAD;

                area.x1 = x1 + band.left + FILTER_BW_TEXT_MARGIN;
                area.y1 = ty;
                area.x2 = x1 + band.right - FILTER_BW_TEXT_MARGIN;
                area.y2 = ty + txt_size.y;

                lv_draw_label(draw_ctx, &label_dsc, &area, txt, NULL);
            }
        }
    }

    /* Notch */
    if (f->dnf_mode == x6200_dnf_manual) {
        int32_t from, to;

        rect_dsc.bg_color = lv_color_white();
        rect_dsc.bg_opa   = LV_OPA_50;

        from = sign_from * (f->dnf_center - f->dnf_width);
        to   = sign_to * (f->dnf_center + f->dnf_width);

        if (from < to) {
            f1 = (w * from) / w_hz;
            f2 = (w * to) / w_hz;
        } else {
            f1 = (w * to) / w_hz;
            f2 = (w * from) / w_hz;
        }

        area.x1 = x1 + w / 2 + f1;
        area.y1 = y1 + h - visor_height;
        area.x2 = x1 + w / 2 + f2;
        area.y2 = y1 + h;

        lv_draw_rect(draw_ctx, &rect_dsc, &area);
    }

    if (rtty_get_state() != RTTY_OFF) {
        int32_t from, to;

        from = sign_from * (params.rtty_center - params.rtty_shift / 2);
        to   = sign_to * (params.rtty_center + params.rtty_shift / 2);

        f1 = (int64_t)(w * from) / w_hz;
        f2 = (int64_t)(w * to) / w_hz;

        main_a.x = x1 + w / 2 + f1;
        main_a.y = y1 + h - visor_height;
        main_b.x = main_a.x;
        main_b.y = y1 + h;
        lv_draw_line(draw_ctx, &main_line_dsc, &main_a, &main_b);

        main_a.x = x1 + w / 2 + f2;
        main_b.x = main_a.x;
        lv_draw_line(draw_ctx, &main_line_dsc, &main_a, &main_b);
    }

    /* Center */

    main_line_dsc.width = 1;

    main_a.x = x1 + w / 2;
    main_a.y = y1 + h - visor_height;
    main_b.x = main_a.x;
    main_b.y = y1 + h;

    if (recorder_is_on()) {
        main_line_dsc.color = lv_color_hex(theme_hex(TC_tx));
    } else if (f->mode == x6200_mode_cw || f->mode == x6200_mode_cwr) {
        // Hide LO line on CW
        main_line_dsc.opa = LV_OPA_0;
    }

    lv_draw_line(draw_ctx, &main_line_dsc, &main_a, &main_b);
}

static void tx_cb(lv_event_t *e) {
    visor_height = VISOR_HEIGHT_TX;
}

static void rx_cb(lv_event_t *e) {
    visor_height = VISOR_HEIGHT_RX;
}

/* Pide un refresco del espectro con el mismo ticket que spectrum_data(): a lo
 * sumo uno encolado a la vez, y si la cola del scheduler esta llena se devuelve
 * el ticket para reintentar. Lo usan los observers de config (filtro real, BW,
 * modo, ancho de FFT): el redibujo se difiere al hilo de GUI y nunca se acumula
 * una cola sin limite (issue #173, PR 1; contrato de #149). */
static void spectrum_enqueue_refresh(void) {
    bool enqueue;

    pthread_mutex_lock(&data_mux);
    enqueue = !refresh_queued;
    refresh_queued = true;
    pthread_mutex_unlock(&data_mux);

    if (enqueue && !scheduler_put_noargs(spectrum_refresh)) {
        pthread_mutex_lock(&data_mux);
        refresh_queued = false;
        pthread_mutex_unlock(&data_mux);
    }
}

static void spectrum_refresh(void *data) {
    /* El ticket se libera apenas el hilo de GUI toma el refresco: si la GUI se
     * atrasa, el productor vuelve a encolar y el frame viejo no se dibuja. */
    pthread_mutex_lock(&data_mux);
    refresh_queued = false;
    pthread_mutex_unlock(&data_mux);

    refresh_counter++;

    if (refresh_counter >= refresh_period) {
        refresh_counter = 0;
        lv_obj_invalidate(obj);
    }
}

/* Cadencia del redibujo: la fija el perfil visual, que corre en el hilo de LVGL
 * (apply_visual_profile via scheduler y apply_visual_override_at_boot en
 * radio_init), el mismo hilo que spectrum_refresh: no cruza hilos. */
void spectrum_set_refresh_period(uint8_t frames) {
    if (frames == 0) {
        return;
    }

    refresh_period = frames;
}

lv_obj_t *spectrum_init(lv_obj_t *parent) {
    spectrum_min_max_reset();

    for (size_t i = 0; i < SPECTRUM_SIZE; i++) {
        frame.peak[i].val = S_MIN;
        frame.buf[i] = S_MIN;
    }

    obj = lv_obj_create(parent);

    lv_obj_add_style(obj, &spectrum_style, 0);
    lv_obj_add_event_cb(obj, spectrum_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(obj, tx_cb, EVENT_RADIO_TX, NULL);
    lv_obj_add_event_cb(obj, rx_cb, EVENT_RADIO_RX, NULL);

    subject_add_observer_and_call(cfg_cur.fft_width, on_fft_width_changed, NULL);
    subject_add_observer_and_call(cfg_cur.filter.real.from, on_real_filter_from_change, NULL);
    subject_add_observer_and_call(cfg_cur.filter.real.to, on_real_filter_to_change, NULL);
    subject_add_observer_and_call(cfg_cur.filter.bw, on_filter_bw_change, NULL);
    subject_add_observer_and_call(cfg_cur.mode, on_cur_mode_change, NULL);
    subject_add_observer_and_call(cfg_cur.band->grid.min.val, on_grid_min_change, NULL);
    subject_add_observer_and_call(cfg_cur.band->grid.max.val, on_grid_max_change, NULL);

    subject_add_observer_and_call(cfg.dnf.val, on_int32_val_change, &frame.dnf_mode);
    subject_add_observer_and_call(cfg.dnf_center.val, on_int32_val_change, &frame.dnf_center);
    subject_add_observer_and_call(cfg.dnf_width.val, on_int32_val_change, &frame.dnf_width);

    subject_add_observer_and_call(cfg_cur.fg_freq, on_cur_freq_change, NULL);
    return obj;
}

void spectrum_data(float *data_buf, uint16_t size, bool tx) {
    uint64_t now = get_time();
    bool     enqueue;

    /* Bounds: el frame tiene SPECTRUM_SIZE muestras. El llamador pasa
     * SPECTRUM_NFFT (== SPECTRUM_SIZE, src/dsp.h); un size mayor se recorta aca
     * en vez de escribir fuera del buffer. */
    if (size > SPECTRUM_SIZE) {
        size = SPECTRUM_SIZE;
    }

    pthread_mutex_lock(&data_mux);

    frame.tx = tx;

    for (uint16_t i = 0; i < size; i++) {
        frame.buf[i] = tx ? data_buf[i] - 30.0f : data_buf[i];

        if (params.spectrum_peak && !tx) {
            float   v    = frame.buf[i];
            peak_t *peak = &frame.peak[i];

            if (v > peak->val) {
                peak->time = now;
                peak->val  = v;
            } else {
                if (now - peak->time > params.spectrum_peak_hold) {
                    peak->val -= params.spectrum_peak_speed;
                }
            }
        }
    }

    enqueue = !refresh_queued;
    refresh_queued = true;

    pthread_mutex_unlock(&data_mux);

    /* Coalescing: si ya hay un refresco encolado, este frame no encola otro (el
     * que esta en cola dibuja el frame mas nuevo). Si el scheduler rechaza el
     * trabajo (cola llena), se devuelve el ticket para que el proximo frame lo
     * reintente: el espectro no puede quedarse sin refrescos. */
    if (enqueue && !scheduler_put_noargs(spectrum_refresh)) {
        pthread_mutex_lock(&data_mux);
        refresh_queued = false;
        pthread_mutex_unlock(&data_mux);
    }
}

void spectrum_min_max_reset() {
    pthread_mutex_lock(&data_mux);

    if (params.spectrum_auto_min.x) {
        frame.grid_min = DEFAULT_MIN;
    } else {
        frame.grid_min = subject_get_int(cfg_cur.band->grid.min.val);
    }
    if (params.spectrum_auto_max.x) {
        frame.grid_max = DEFAULT_MAX;
    } else {
        frame.grid_max = subject_get_int(cfg_cur.band->grid.max.val);
    }

    pthread_mutex_unlock(&data_mux);
}

float spectrum_get_min() {
    float v;

    pthread_mutex_lock(&data_mux);
    v = frame.grid_min;
    pthread_mutex_unlock(&data_mux);

    return v;
}

void spectrum_update_max(float db) {
    pthread_mutex_lock(&data_mux);

    if (params.spectrum_auto_max.x) {
        lpf(&frame.grid_max, db + 10.0f, 0.55f, DEFAULT_MAX);
    } else {
        // TODO: set min/max at param change
        frame.grid_max = subject_get_int(cfg_cur.band->grid.max.val);
    }

    pthread_mutex_unlock(&data_mux);
}

void spectrum_update_min(float db) {
    pthread_mutex_lock(&data_mux);

    if (params.spectrum_auto_min.x) {
        lpf(&frame.grid_min, db + 3.0f, 0.75f, DEFAULT_MIN);
    } else {
        frame.grid_min = subject_get_int(cfg_cur.band->grid.min.val);
    }

    pthread_mutex_unlock(&data_mux);
}

void spectrum_clear() {
    // spectrum_min_max_reset();
    uint64_t now = get_time();

    pthread_mutex_lock(&data_mux);

    freq_mod = 0;

    for (uint16_t i = 0; i < SPECTRUM_SIZE; i++) {
        frame.buf[i]       = S_MIN;
        frame.peak[i].val  = S_MIN;
        frame.peak[i].time = now;
    }

    pthread_mutex_unlock(&data_mux);
}

static void on_fft_width_changed(Subject *subj, void *user_data) {
    int32_t w = subject_get_int(subj);

    pthread_mutex_lock(&data_mux);
    frame.width_hz = w;
    pthread_mutex_unlock(&data_mux);

    spectrum_clear();
    spectrum_enqueue_refresh();
}

static void on_real_filter_from_change(Subject *subj, void *user_data) {
    int32_t v = subject_get_int(subj);

    pthread_mutex_lock(&data_mux);
    frame.filter_from = v;
    pthread_mutex_unlock(&data_mux);

    spectrum_enqueue_refresh();
}

static void on_real_filter_to_change(Subject *subj, void *user_data) {
    int32_t v = subject_get_int(subj);

    pthread_mutex_lock(&data_mux);
    frame.filter_to = v;
    pthread_mutex_unlock(&data_mux);

    spectrum_enqueue_refresh();
}

static void on_filter_bw_change(Subject *subj, void *user_data) {
    int32_t v = subject_get_int(subj);

    pthread_mutex_lock(&data_mux);
    frame.filter_bw = v;
    pthread_mutex_unlock(&data_mux);

    spectrum_enqueue_refresh();
}

static void on_cur_mode_change(Subject *subj, void *user_data) {
    x6200_mode_t m = (x6200_mode_t) subject_get_int(subj);

    pthread_mutex_lock(&data_mux);
    frame.mode = m;
    pthread_mutex_unlock(&data_mux);

    spectrum_enqueue_refresh();
}

static void on_grid_min_change(Subject *subj, void *user_data) {
    if (!params.spectrum_auto_min.x) {
        int32_t v = subject_get_int(subj);

        pthread_mutex_lock(&data_mux);
        frame.grid_min = v;
        pthread_mutex_unlock(&data_mux);
    }
}
static void on_grid_max_change(Subject *subj, void *user_data) {
    if (!params.spectrum_auto_max.x) {
        int32_t v = subject_get_int(subj);

        pthread_mutex_lock(&data_mux);
        frame.grid_max = v;
        pthread_mutex_unlock(&data_mux);
    }
}

static void on_int32_val_change(Subject *subj, void *user_data) {
    int32_t v = subject_get_int(subj);

    pthread_mutex_lock(&data_mux);
    *(int32_t*)user_data = v;
    pthread_mutex_unlock(&data_mux);
}

void on_cur_freq_change(Subject *subj, void *user_data) {
    int32_t new_freq = subject_get_int(subj);

    pthread_mutex_lock(&data_mux);

    if (cur_freq != new_freq) {
        int32_t df = new_freq - cur_freq + freq_mod;
        cur_freq = new_freq;
        uint64_t time = get_time();

        uint16_t div     = frame.width_hz / SPECTRUM_SIZE;
        int32_t  delta   = (df + div / 2) / div;
        freq_mod = df - delta * div;

        if (delta != 0) {
            peak_t  *to;
            for (int16_t i = 0; i < SPECTRUM_SIZE; i++) {
                int16_t dst_id = delta > 0 ? i : SPECTRUM_SIZE - i - 1;
                to = &frame.peak[dst_id];
                int16_t src_id = dst_id + delta;
                if ((src_id < 0) || (src_id >= SPECTRUM_SIZE)) {
                    to->val = S_MIN;
                    to->time = time;
                } else {
                    *to = frame.peak[src_id];
                }
            }
        }
    }

    pthread_mutex_unlock(&data_mux);
}

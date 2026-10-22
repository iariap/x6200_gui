/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */
#include "waterfall.h"
#include "theme.h"

#include "spectrum.h"
#include "styles.h"
#include "radio.h"
#include "events.h"
#include "params/params.h"
#include "band_info.h"
#include "meter.h"
#include "backlight.h"
#include "dsp.h"
#include "util.h"
#include "pubsub_ids.h"
#include "scheduler.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define PX_BYTES    sizeof(lv_color_t)
#define DEFAULT_MIN S7
#define DEFAULT_MAX S9_40
#define WIDTH 800

/* Alto de los dos ticks de la banda pasante RX sobre el borde superior de la
 * cascada (issue #173, PR 1). Overlay de UI: no toca el buffer de datos ni el
 * anillo de filas rasterizadas. */
#define FILTER_TICK_H 6

typedef struct {
    uint8_t data[WATERFALL_NFFT];
    uint32_t width_hz;
    int32_t center_freq;
} waterfall_cache_row_t;

static lv_obj_t         *obj;
static lv_obj_t         *img;

static lv_style_t       middle_line_style;
static lv_obj_t         *middle_line;
static lv_point_t       middle_line_points[] = { {0, 0}, {0, 0} };

/* Ticks del ancho del filtro RX (issue #173, PR 1): dos marcas verticales finas
 * en el borde superior de la cascada, en el rol `vfo_line` del tema. Hijos del
 * objeto del waterfall (overlay), creados ANTES de la linea central para que
 * esta siga siendo el ultimo hijo (invariante del #140). */
static lv_style_t       filter_tick_style;
static lv_obj_t        *filter_tick[2];
static lv_point_t       filter_tick_points[] = { {0, 0}, {0, FILTER_TICK_H} };
static atomic_bool      filter_tick_queued;

static lv_coord_t       height;
static int32_t         width_hz = 100000;

static float            grid_min = DEFAULT_MIN;
static float            grid_max = DEFAULT_MAX;

static lv_img_dsc_t     *frame;

// static int32_t          *freq_offsets;
static uint16_t               last_row_id;
static waterfall_cache_row_t *waterfall_cache;

/* OPT-2 (#148): anillo de filas YA rasterizadas dentro de `frame`.
 *
 * `render_head` es el indice de la fila mas nueva. La fila `j` de pantalla
 * (j=0 arriba) muestra `frame[(render_head + j) % height]`, y el scroll se
 * resuelve con `lv_img_set_offset_y()`: en LVGL 8.3.11, cuando la fuente no es
 * mayor que el objeto, el offset recorre la imagen y la envuelve (lv_img.c,
 * `offset_y % img->h` + el bucle de tiles), asi que el anillo se muestra sin
 * mover un solo byte. `pending_rows` cuenta las filas que el hilo de radio ya
 * escribio y que todavia no se rasterizaron; el wake (panel encendido) las
 * muestra TODAS, aunque `refresh_period > 1`. */
static uint16_t         render_head;
static uint32_t         pending_rows;

/* Parametros de mapeo de la ultima rasterizacion. Si cambian, hay que
 * reconstruir las `height` filas (el mapeo por fila depende de ellos). La paleta
 * se compara por contenido porque los temas JSON comparten el mismo array
 * (`theme_waterfall()` devuelve el `palette` estatico de theme.c). */
static bool             rendered_valid;
static int32_t          rendered_center_freq;
static uint32_t         rendered_width_hz;
static uint32_t         rendered_palette[256];

static int32_t          radio_center_freq = 0;
static int32_t          wf_center_freq = 0;

static uint8_t          refresh_period = 1;
static uint8_t          refresh_counter = 0;

/* Instrumentos del redibujo offscreen (issue #13, OPT-2 #148):
 *  - `offscreen_redraws`: refrescos de la cascada que corrieron (mismo contrato
 *    del issue #13: 1 por fila con el panel encendido, 0 con el panel apagado).
 *    Lo lee `--bench-natural --display-off` (tests/natural_evidence.py).
 *  - `rasterized_px`: pixeles efectivamente mapeados (division + paleta). Con el
 *    anillo es WIDTH por fila nueva en reposo, contra WIDTH*height por fila del
 *    baseline; en una reconstruccion completa vuelve a WIDTH*height.
 *  - `full_rebuilds`: cuantas reconstrucciones completas corrieron. */
static uint32_t         offscreen_redraws = 0;
static uint64_t         rasterized_px = 0;
static uint32_t         full_rebuilds = 0;


static void refresh_waterfall( void * arg);
static void draw_middle_line();
static void draw_filter_ticks(bool create);
static void filter_tick_job(void *arg);
static void on_filter_from_change(Subject *subj, void *user_data);
static void on_filter_to_change(Subject *subj, void *user_data);
static void render_row(uint16_t dst_y, const waterfall_cache_row_t *row);
static void render_all();
static void render_pending(uint32_t count);
static void on_fg_freq_change(Subject *subj, void *user_data);
static void on_grid_min_change(Subject *subj, void *user_data);
static void on_grid_max_change(Subject *subj, void *user_data);
static void on_fft_width_changed(Subject *subj, void *user_data);


/* La vista actual (centro y mitad del ancho), para las marcas de spots */
void waterfall_view(int32_t *center, int32_t *half) {
    if (center != NULL) {
        *center = wf_center_freq;
    }

    if (half != NULL) {
        *half = width_hz / 2;
    }
}

lv_obj_t * waterfall_init(lv_obj_t * parent) {
    subject_add_observer_and_call(cfg_cur.fg_freq, on_fg_freq_change, NULL);
    wf_center_freq = radio_center_freq;

    obj = lv_obj_create(parent);

    /* La linea central pertenece a ESTE waterfall: waterfall_init() arma un obj
     * nuevo por pantalla, asi que el puntero que quedo de una pantalla anterior
     * ya no sirve. draw_middle_line() crea la linea una sola vez por obj (issue
     * #140), y sin este reset reusaria el objeto viejo. Los ticks del filtro
     * siguen el mismo patron. */
    middle_line   = NULL;
    filter_tick[0] = NULL;
    filter_tick[1] = NULL;

    lv_obj_add_style(obj, &waterfall_style, 0);
    if (obj) lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    /* Linea central del waterfall (orden del owner, 2026-10-18): "no veo la
     * frecuencia que estoy sintonizando; no se ve la linea". El ancho sale del tema
     * (3 px por default), a opacidad plena y en el rol `text` del tema: sobre el
     * fondo del waterfall los seis temas miden 11.8:1..16.4:1 (antes, 60% + ADDITIVE
     * daba 1.2:1..3.2:1 y en Sunlit se saturaba a blanco). No se usa ADDITIVE porque
     * con LV_OPA_COVER el compuesto es `fondo + color` y satura a blanco sobre un
     * piso claro (medido: Sunlit (165,208,248) sobre (225,235,240), 1.2:1). */
    lv_style_init(&middle_line_style);
    lv_style_set_line_width(&middle_line_style, theme_shape.waterfall_signal_width);
    lv_style_set_line_color(&middle_line_style, lv_color_hex(theme_hex(TC_text)));
    lv_style_set_line_opa(&middle_line_style, LV_OPA_COVER);

    /* Ticks del filtro (issue #173, PR 1): finos (1 px), en el rol `vfo_line`
     * del tema (el mismo de la banda del espectro), opacidad plena. */
    lv_style_init(&filter_tick_style);
    lv_style_set_line_width(&filter_tick_style, 1);
    lv_style_set_line_color(&filter_tick_style, lv_color_hex(theme_hex(TC_vfo_line)));
    lv_style_set_line_opa(&filter_tick_style, LV_OPA_COVER);

    subject_add_observer_and_call(cfg_cur.band->grid.min.val, on_grid_min_change, NULL);
    subject_add_observer_and_call(cfg_cur.band->grid.max.val, on_grid_max_change, NULL);
    subject_add_observer_and_call(cfg_cur.fft_width, on_fft_width_changed, NULL);
    subject_add_observer_and_call(cfg_cur.filter.real.from, on_filter_from_change, NULL);
    subject_add_observer_and_call(cfg_cur.filter.real.to, on_filter_to_change, NULL);
    return obj;
}

static void scroll_down() {
    last_row_id = (last_row_id + 1) % height;
}

void waterfall_data(float *data_buf, uint16_t size, bool tx) {
    /* Limites (issue #148): sin alto no hay anillo, y `size` no puede pasar la
     * capacidad real del array de la fila (WATERFALL_NFFT). El ROM garantiza
     * size <= WATERFALL_NFFT (dsp.cpp: waterfall_avg_data->size()), pero el
     * modulo no depende de esa precondicion para no escribir fuera del buffer. */
    if (height == 0 || waterfall_cache == NULL) {
        return;
    }
    if (size > WATERFALL_NFFT) {
        size = WATERFALL_NFFT;
    }

    scroll_down();

    float min, max;
    if (tx) {
        min = DEFAULT_MIN;
        max = DEFAULT_MAX;
    } else {
        min = grid_min;
        max = grid_max;
    }

    waterfall_cache[last_row_id].center_freq = radio_center_freq;
    waterfall_cache[last_row_id].width_hz = tx ? 48000 : width_hz;

    for (uint16_t x = 0; x < size; x++) {
        float       v = (data_buf[x] - min) / (max - min);

        if (v < 0.0f) {
            v = 0.0f;
        } else if (v > 1.0f) {
            v = 1.0f;
        }

        uint8_t id = v * 255;
        waterfall_cache[last_row_id].data[x] = id;
    }

    /* El hilo de radio solo publica la fila; el rasterizado corre en el hilo de
     * GUI (scheduler_put_noargs -> refresh_waterfall), como en el baseline. */
    pending_rows++;
    scheduler_put_noargs(refresh_waterfall);
}

static void do_scroll_cb(lv_event_t * event) {
    if (wf_center_freq == radio_center_freq) {
        return;
    }
    if (params.waterfall_smooth_scroll.x) {
        wf_center_freq += (radio_center_freq - wf_center_freq) / 10 + 1;
    } else {
        wf_center_freq = radio_center_freq;
    }
    scheduler_put_noargs(refresh_waterfall);
}

void waterfall_set_height(lv_coord_t h) {
    lv_obj_set_height(obj, h);
    lv_obj_update_layout(obj);

    /* For more accurate horizontal scroll, it should be a "multiple of 500Hz" */
    /* 800 * 500Hz / 100000Hz = 4.0px */

    height = lv_obj_get_height(obj);

    /* Resize seguro (issue #148): liberar el frame/cache/img anteriores antes de
     * rearmar, para que una segunda llamada no filtre ni deje el objeto viejo.
     * Los ticks del filtro son hijos de la imagen (issue #173): se borran con
     * ella, asi que hay que soltar los punteros. */
    if (img != NULL) {
        lv_obj_del(img);
        img = NULL;
        filter_tick[0] = NULL;
        filter_tick[1] = NULL;
    }
    if (frame != NULL) {
        lv_img_buf_free(frame);
        frame = NULL;
    }
    if (waterfall_cache != NULL) {
        free(waterfall_cache);
        waterfall_cache = NULL;
    }

    render_head = 0;
    pending_rows = 0;
    rendered_valid = false;

    /* Alto 0 (limite): no hay imagen; el resto del modulo queda inerte. */
    if (height <= 0) {
        return;
    }

    frame = lv_img_buf_alloc(WIDTH, height, LV_IMG_CF_TRUE_COLOR);

    if (frame == NULL) {
        height = 0;
        return;
    }

    img = lv_img_create(obj);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_img_set_src(img, frame);

    last_row_id = 0;

    waterfall_cache = calloc(sizeof(waterfall_cache[0]), height);

    if (waterfall_cache == NULL) {
        lv_img_buf_free(frame);
        frame = NULL;
        height = 0;
        return;
    }

    for (size_t i = 0; i < height; i++) {
        waterfall_cache[i].center_freq = radio_center_freq;
        waterfall_cache[i].width_hz = width_hz;
    }

    lv_obj_add_event_cb(img, do_scroll_cb, LV_EVENT_DRAW_POST_END, NULL);

    waterfall_min_max_reset();
    band_info_init(obj);

    /* Los ticks del filtro se crean ANTES de la linea central: la linea central
     * tiene que seguir siendo el ultimo hijo del waterfall (invariante #140). */
    draw_filter_ticks(true);
    draw_middle_line();
}

static void middle_line_cb(lv_event_t * event) {
    if (middle_line == NULL) {
        return;
    }

    bool hidden = lv_obj_has_flag(middle_line, LV_OBJ_FLAG_HIDDEN);

    if (params.waterfall_center_line.x && hidden) {
        lv_obj_clear_flag(middle_line, LV_OBJ_FLAG_HIDDEN);
    } else if (!params.waterfall_center_line.x && !hidden) {
        lv_obj_add_flag(middle_line, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Linea central del waterfall (issue #140).
 *
 * Los dos puntos salen de `height`: el inferior es el alto del waterfall, asi que
 * la linea solo se puede armar cuando waterfall_set_height() ya lo resolvio (el
 * init corre antes y ahi `height` todavia vale 0). Por eso se crea desde aca y no
 * desde waterfall_init(), y se crea UNA sola vez por obj: una linea nueva por
 * llamada dejaria la anterior con el alto viejo y el callback duplicado. Cada
 * llamada vuelve a fijar el punto inferior (el alto puede cambiar) y resuelve la
 * visibilidad con params.waterfall_center_line en el acto, sin esperar al proximo
 * DRAW_POST_END; el callback la mantiene al dia cuando el parametro cambia en
 * Ajustes. */
static void draw_middle_line() {
    middle_line_points[1].y = height;

    bool created = false;

    if (middle_line == NULL) {
        middle_line = lv_line_create(obj);
        created = true;
    }

    lv_line_set_points(middle_line, middle_line_points, 2);

    if (created) {
        lv_obj_add_style(middle_line, &middle_line_style, 0);
        lv_obj_add_event_cb(obj, middle_line_cb, LV_EVENT_DRAW_POST_END, NULL);
    }

    lv_obj_center(middle_line);

    if (params.waterfall_center_line.x) {
        lv_obj_clear_flag(middle_line, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(middle_line, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Posiciona (y, si `create`, crea) los dos ticks del filtro RX en el borde
 * superior de la cascada. Corre siempre en el hilo de GUI: desde
 * waterfall_set_height() o desde el job diferido del scheduler. Lee los bordes
 * reales de RF (cfg_cur.filter.real.from/to) con el MISMO spectrum_filter_band()
 * que el espectro, asi los dos coinciden; el overlay NUNCA toca el buffer de
 * datos ni el anillo de filas. */
static void draw_filter_ticks(bool create) {
    if (img == NULL || height <= 0) {
        return;
    }

    spectrum_filter_band_t band = spectrum_filter_band(subject_get_int(cfg_cur.filter.real.from),
                                                       subject_get_int(cfg_cur.filter.real.to),
                                                       (int32_t) width_hz, WIDTH);

    /* Los ticks son hijos de la IMAGEN del waterfall (su superficie de pixeles,
     * de 800 px, alineada con la del espectro), no del objeto, que tiene padding:
     * asi el tick cae exactamente en la columna de pixeles de la banda. */
    for (int i = 0; i < 2; i++) {
        if (filter_tick[i] == NULL) {
            if (!create) {
                continue;
            }

            filter_tick[i] = lv_line_create(img);
            lv_obj_add_style(filter_tick[i], &filter_tick_style, 0);
            lv_obj_clear_flag(filter_tick[i], LV_OBJ_FLAG_CLICKABLE);
            lv_line_set_points(filter_tick[i], filter_tick_points, 2);
        }

        if (!band.visible) {
            lv_obj_add_flag(filter_tick[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(filter_tick[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(filter_tick[i], (i == 0) ? band.left : band.right, 0);
    }
}

static void filter_tick_job(void *arg) {
    (void) arg;

    atomic_store(&filter_tick_queued, false);
    draw_filter_ticks(false);
}

/* Coalescing: a lo sumo un job de ticks encolado; si la cola del scheduler esta
 * llena, se devuelve el ticket. El job corre en el hilo de GUI
 * (scheduler_put_noargs), nunca desde el observer que notifica. */
static void request_filter_ticks(void) {
    if (!atomic_exchange(&filter_tick_queued, true)) {
        if (!scheduler_put_noargs(filter_tick_job)) {
            atomic_store(&filter_tick_queued, false);
        }
    }
}

static void on_filter_from_change(Subject *subj, void *user_data) {
    (void) subj; (void) user_data;

    request_filter_ticks();
}

static void on_filter_to_change(Subject *subj, void *user_data) {
    (void) subj; (void) user_data;

    request_filter_ticks();
}

/* Geometria de la linea central para el arnes de UI (mismo espiritu que
 * waterfall_offscreen_redraw_count): el gate nativo no puede mirar pixeles y el
 * invariante del issue #140 es geometrico (largo = alto del waterfall). */
lv_obj_t * waterfall_middle_line(void) {
    return middle_line;
}

/* Ticks del filtro para el arnes de UI (issue #173, PR 1): el arnes verifica que
 * existan dos, que sean hijos del waterfall y que su geometria corresponda a
 * spectrum_filter_band(). NULL mientras no se armaron. */
lv_obj_t * waterfall_filter_tick(int i) {
    if (i < 0 || i > 1) {
        return NULL;
    }

    return filter_tick[i];
}

void waterfall_min_max_reset() {
    if (params.waterfall_auto_min.x) {
        grid_min = DEFAULT_MIN;
    } else {
        grid_min = subject_get_int(cfg_cur.band->grid.min.val);
    }
    if (params.waterfall_auto_max.x) {
        grid_max = DEFAULT_MAX;
    } else {
        grid_max = subject_get_int(cfg_cur.band->grid.max.val);
    }
}

void waterfall_update_max(float db) {
    if (params.waterfall_auto_max.x) {
        lpf(&grid_max, db + 3.0f, 0.85f, DEFAULT_MAX);
    } else {
        // TODO: set min/max at param change
        grid_max = subject_get_int(cfg_cur.band->grid.max.val);
    }
}

void waterfall_update_min(float db) {
    if (params.waterfall_auto_min.x) {
        lpf(&grid_min, db + 3.0f, 0.95f, DEFAULT_MIN);
    } else {
        grid_min = subject_get_int(cfg_cur.band->grid.min.val);
    }
}

void waterfall_refresh_reset() {
    refresh_period = 1;
}

void waterfall_refresh_period_set(uint8_t k) {
    if (k == 0) {
        return;
    }
    refresh_period = k;
}

void waterfall_set_battery_mode(bool enabled) {
    /* In battery mode, skip 3 out of 4 frames (75% reduction) */
    refresh_period = enabled ? 4 : 1;
}

/* Rasteriza UNA fila del anillo a la fila `dst_y` del frame. El mapeo es el
 * mismo del baseline (el viejo `redraw_cb`): no cambia ni un pixel, solo CUANDO
 * se corre cada fila. `rasterized_px` cuenta los px efectivamente mapeados
 * (division + lookup de paleta) para medir el ahorro sin vender el flush. */
static void render_row(uint16_t dst_y, const waterfall_cache_row_t *row) {
    int32_t hz_to_pix_divisor = width_hz / WIDTH;

    /* Ancho en Hz estrecho: `width_hz / WIDTH` nunca puede ser divisor 0
     * (issue #148). Con menos de 800 Hz de span el mapeo queda saturado a 1 px/Hz
     * en vez de dividir por cero (UB en el baseline). */
    if (hz_to_pix_divisor < 1) {
        hz_to_pix_divisor = 1;
    }

    lv_color_t black = lv_color_black();
    lv_color_t *dst = (lv_color_t *) frame->data + (size_t) dst_y * WIDTH;

    int32_t left_freq = row->center_freq - (int32_t) row->width_hz / 2;
    int32_t right_freq = left_freq + (int32_t) row->width_hz;

    left_freq -= wf_center_freq;
    right_freq -= wf_center_freq;

    // hz to px
    left_freq = left_freq / hz_to_pix_divisor + WIDTH / 2;
    right_freq = right_freq / hz_to_pix_divisor + WIDTH / 2;
    int32_t src_w = right_freq - left_freq;

    if ((right_freq < 0) || (left_freq > WIDTH) || (src_w <= 0)) {
        /* Fila fuera de vista (o mapeo degenerado): negra, como el baseline. */
        memset(dst, 0, WIDTH * PX_BYTES);
        return;
    }

    for (int32_t dst_x = 0; dst_x < WIDTH; dst_x++) {
        uint16_t src_x = (dst_x - left_freq) * WATERFALL_NFFT / src_w;
        lv_color_t px_color;
        if ((src_x < 0) || (src_x >= WATERFALL_NFFT - 1)) {
            px_color = black;
        } else {
            uint8_t y_p = row->data[src_x];
            px_color = (lv_color_t)wf_palette[y_p];
        }
        dst[dst_x] = px_color;
    }

    rasterized_px += WIDTH;
}

/* Reconstruccion completa: rasteriza las `height` filas visibles (la ventana
 * termina en `last_row_id`). `render_head` no cambia: la fila mas nueva sigue en
 * su slot. */
static void render_all() {
    for (int32_t j = 0; j < height; j++) {
        uint16_t cache_id = (uint16_t) ((last_row_id + height - j) % height);
        uint16_t slot = (uint16_t) ((render_head + j) % height);
        render_row(slot, &waterfall_cache[cache_id]);
    }
}

/* Camino incremental: rasteriza solo las `count` filas nuevas (de la mas vieja a
 * la mas nueva), cada una en el slot nuevo del anillo. */
static void render_pending(uint32_t count) {
    for (uint32_t k = 0; k < count; k++) {
        uint16_t cache_id = (uint16_t) ((last_row_id + height - count + 1 + k) % height);
        render_head = (uint16_t) ((render_head + height - 1) % height);
        render_row(render_head, &waterfall_cache[cache_id]);
    }
}

static void refresh_waterfall( void * arg) {
    refresh_counter++;

    if (refresh_counter < refresh_period) {
        return;
    }

    refresh_counter = 0;

    /* Pantalla apagada (issue #13, UI-07 remanente): `backlight_switch()` apago
     * la invalidacion del display (src/backlight.c), asi que el redibujo de la
     * paleta fuera de pantalla no llega a ningun lado. Se saltea mientras el
     * panel esta oscuro; al volver a encender, el proximo refresh rasteriza
     * TODAS las filas pendientes (no se pierde ninguna). */
    if (!backlight_is_on()) {
        return;
    }

    if (height == 0 || frame == NULL || waterfall_cache == NULL) {
        return;
    }

    offscreen_redraws++;

    /* Reconstruir solo cuando el mapeo por fila cambio: centro (scroll o salto),
     * ancho de FFT, paleta/tema, primer render, o el historial es incompatible
     * (llegaron >= height filas sin refrescar, asi que algunas ya se pisaron).
     * min/max y TX NO exigen reconstruir: los ids de la cache ya estan
     * normalizados al escribir la fila, y el ancho/centro de la fila TX viajan
     * en la propia fila. */
    bool params_changed = !rendered_valid
        || wf_center_freq != rendered_center_freq
        || width_hz != rendered_width_hz
        || memcmp(wf_palette, rendered_palette, sizeof(rendered_palette)) != 0;

    uint32_t pending = pending_rows;
    pending_rows = 0;

    if (params_changed || pending >= (uint32_t) height) {
        render_all();
        full_rebuilds++;
        rendered_valid = true;
    } else if (pending > 0) {
        render_pending(pending);
        rendered_valid = true;
    } else {
        return;   /* nada nuevo que mostrar */
    }

    rendered_center_freq = wf_center_freq;
    rendered_width_hz = width_hz;
    memcpy(rendered_palette, wf_palette, sizeof(rendered_palette));

    /* Scroll por indexacion: el offset de LVGL envuelve el anillo (ver el
     * comentario de `render_head`). `lv_img_set_offset_y` ya invalida el objeto. */
    lv_img_set_offset_y(img, (height - render_head) % height);
    lv_obj_invalidate(img);
}

uint32_t waterfall_offscreen_redraw_count(void) {
    return offscreen_redraws;
}

/* Instrumento OPT-2 (#148): pixeles mapeados (division + paleta) desde el
 * arranque. El arnes lo reporta en el bloque `natural`. */
uint64_t waterfall_rasterized_px(void) {
    return rasterized_px;
}

/* Reconstrucciones completas de la paleta (mapeo cambiado / historial
 * incompatible), para el informe A/B. */
uint32_t waterfall_full_rebuild_count(void) {
    return full_rebuilds;
}

/* Indice de la fila mas nueva dentro del frame (la ventana visible empieza ahi),
 * para que los fixtures aserten el offset del anillo. */
uint16_t waterfall_render_head(void) {
    return render_head;
}

/* La imagen rasterizada del anillo (WIDTH x height px, TRUE_COLOR 32 bits) y su
 * alto, para que los fixtures OPT-2 (#148) comparen el framebuffer interno byte
 * a byte contra el baseline. NULL si todavia no se armo. */
const lv_color_t *waterfall_frame_pixels(void) {
    return (frame != NULL) ? (const lv_color_t *) frame->data : NULL;
}

lv_coord_t waterfall_frame_height(void) {
    return height;
}

static void on_fg_freq_change(Subject *subj, void *user_data) {
    radio_center_freq = subject_get_int(subj);
}

static void on_grid_min_change(Subject *subj, void *user_data) {
    if (!params.waterfall_auto_min.x) {
        grid_min = subject_get_int(subj);
    }
}
static void on_grid_max_change(Subject *subj, void *user_data) {
    if (!params.waterfall_auto_max.x) {
        grid_max = subject_get_int(subj);
    }
}

static void on_fft_width_changed(Subject *subj, void *user_data) {
    width_hz = subject_get_int(subj);

    /* El span cambia la posicion de los ticks del filtro. */
    request_filter_ticks();
}

/* The levels the waterfall is drawing with (the remote scope scales to them) */
void waterfall_get_levels(bool tx, float *min, float *max) {
    (void) tx;

    if (min) { *min = grid_min; }
    if (max) { *max = grid_max; }
}

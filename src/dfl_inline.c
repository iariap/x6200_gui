/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Editor DFL inline (issue #179, correccion del #173; ver src/dfl_inline.h).
 *
 *  Panel NATIVO dentro del rectangulo del waterfall: los 28 px superiores del
 *  waterfall real quedan vivos (las filas mas recientes) y el panel opaco se
 *  dibuja solo desde y_local=28 hacia abajo. Sin dialogo, X, titulo, marco,
 *  sombra ni fondo de ventana. El grafico es protagonista; a la derecha, tres
 *  filas LOW/HIGH/BW con el unico valor exacto en Hz.
 *
 *  Edicion: SOLO el encoder de VOLUMEN (rotary1 -> KEY_VOL_*_EDIT/_SELECT) y el
 *  touch en filas/handles. La rueda principal (rotary0) NO se toca: sigue
 *  sintonizando. Ningun evento del DFL modifica frecuencia ni volumen.
 */

#include "dfl_inline.h"

#include "dfl_rotary.h"

#include "buttons.h"
#include "cfg/cfg.h"
#include "cfg/mode.h"
#include "cfg/mode.private.h"
#include "cfg/subjects.h"
#include "events.h"
#include "info.h"
#include "main.h"
#include "radio.h"
#include "rotary.h"
#include "styles.h"
#include "theme.h"
#include "vol.h"

#include <stdio.h>

/* Alto del micro-waterfall de contexto (C.2/C.3): los 28 px SUPERIORES del
 * waterfall real siguen visibles; el panel se dibuja desde aca hacia abajo. */
#define DFL_MICRO_WF     28

/* Geometria del panel (relativa al waterfall, ancho 800 del X6200). */
#define DFL_LEFT_X       8
#define DFL_LEFT_W       487         /* 8..494 */
#define DFL_SEP_X        499         /* separador de 1 px */
#define DFL_RIGHT_X      505
#define DFL_RIGHT_W      286         /* 505..790 */

#define DFL_HEAD_Y       4
#define DFL_HEAD_H       44          /* target tactil de la cabecera/Reset */

#define DFL_GRAPH_X      DFL_LEFT_X
#define DFL_GRAPH_Y      52
#define DFL_GRAPH_W      DFL_LEFT_W
#define DFL_GRAPH_H      96          /* 52..147 */

#define DFL_ROW_H        44
#define DFL_ROW_Y(i)     (4 + (i) * 48)   /* 4, 52, 100 -> 3 x 44 */

#define DFL_BTN          44          /* target tactil minimo */

/* Margenes internos del area de dibujo del grafico. */
#define GRAPH_PAD_L      8
#define GRAPH_PAD_R      8
#define GRAPH_PAD_T      6
#define GRAPH_AXIS_H     18
#define GRAPH_SKIRT      12

/* Alto minimo del panel para que entren cabecera + grafico + filas. */
#define DFL_PANEL_MIN_H  (DFL_GRAPH_Y + DFL_GRAPH_H)

static lv_obj_t *panel;
static lv_obj_t *graph;
static lv_obj_t *mode_label;
static lv_obj_t *reset_btn;
static lv_obj_t *row[DFL_SEL_COUNT];
static lv_obj_t *row_bar[DFL_SEL_COUNT];
static lv_obj_t *row_name[DFL_SEL_COUNT];
static lv_obj_t *row_value[DFL_SEL_COUNT];

static ObserverDelayed *obs_mode;
static ObserverDelayed *obs_low;
static ObserverDelayed *obs_high;
static ObserverDelayed *obs_bw;

static bool         expanded;
static bool         tx_active;
static dfl_sel_t    sel = DFL_SEL_LOW;
static dfl_handle_t dragging = DFL_HANDLE_NONE;

/* Dominio visual ADAPTATIVO fijado al entrar/cambiar modo (C.3): no se recalcula
 * en cada detent; solo se amplia (nunca se reduce) si el filtro se va afuera. */
static int32_t      domain_span = 10000;

/* Estado previo del encoder VOL, para restaurarlo al colapsar. */
static vol_mode_t   saved_vol_mode = VOL_VOL;
static int          saved_rot_mode = -1;

/* Seam de test: en la radio queda NULL y el arrastre lee el indev de LVGL. */
bool (*dfl_inline_pointer_hook)(lv_point_t *out) = NULL;

/* -------------------------------------------------------------------------- */

static void dfl_refresh(void);

static bool pointer_get(lv_point_t *out) {
    if (dfl_inline_pointer_hook) {
        return dfl_inline_pointer_hook(out);
    }

    lv_indev_t *indev = lv_indev_get_act();

    if (!indev) {
        return false;
    }

    lv_indev_get_point(indev, out);
    return true;
}

static lv_obj_t *make_label(lv_obj_t *parent, int32_t x, int32_t y, const lv_font_t *font,
                            lv_color_t color, int32_t w) {
    lv_obj_t *l = lv_label_create(parent);

    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_pos(l, x, y);

    if (w > 0) {
        lv_obj_set_width(l, w);
    }

    return l;
}

/* Dominio visual actual a partir de la magnitud fijada. */
static dfl_domain_t current_domain(x6200_mode_t mode) {
    return dfl_domain_span(mode, domain_span);
}

/* -------------------------------------------------------------------------- */
/* Aplicaciones: siempre por los setters/caminos vigentes del firmware, via
 * src/dfl_rotary.c (que nunca devuelve un valor optimista: relee el Subject). */

static void apply_low(int32_t val) {
    if (tx_active) {
        return;
    }
    dfl_set_low(val);
    dfl_refresh();
}

static void apply_high(int32_t val) {
    if (tx_active) {
        return;
    }
    dfl_set_high(val);
    dfl_refresh();
}

static void apply_bw(int32_t val) {
    if (tx_active) {
        return;
    }
    dfl_set_bw(val);
    dfl_refresh();
}

static void reset_default(void) {
    if (tx_active) {
        return;
    }

    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    int32_t      def_low;
    int32_t      def_high;
    uint32_t     step;
    uint32_t     zoom;

    if (!mode_default_values(xmode_2_db(mode), &def_low, &def_high, &step, &zoom)) {
        return;
    }

    int32_t cur_low  = subject_get_int(cfg_cur.filter.low);
    int32_t cur_high = subject_get_int(cfg_cur.filter.high);

    if (dfl_reset_low_first(def_low, def_high, cur_low, cur_high)) {
        cfg_mode_set_low_filter(def_low);
        cfg_mode_set_high_filter(def_high);
    } else {
        cfg_mode_set_high_filter(def_high);
        cfg_mode_set_low_filter(def_low);
    }

    dfl_refresh();
}

/* -------------------------------------------------------------------------- */
/* Grafico esquematico (Hz DEL FILTRO, dominio adaptativo con cero visible). */

static void draw_cb(lv_event_t *e) {
    lv_obj_t            *obj      = lv_event_get_target(e);
    lv_draw_ctx_t       *draw_ctx = lv_event_get_draw_ctx(e);
    lv_draw_line_dsc_t   line;
    lv_draw_rect_dsc_t   rect;

    lv_coord_t x1 = obj->coords.x1;
    lv_coord_t y1 = obj->coords.y1;
    lv_coord_t w  = lv_obj_get_width(obj);
    lv_coord_t h  = lv_obj_get_height(obj);

    int32_t px0  = x1 + GRAPH_PAD_L;
    int32_t pw   = w - GRAPH_PAD_L - GRAPH_PAD_R;
    int32_t top  = y1 + GRAPH_PAD_T;
    int32_t base = y1 + h - GRAPH_AXIS_H;

    if (pw <= 0 || base <= top) {
        return;
    }

    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    dfl_domain_t d    = current_domain(mode);
    int32_t      low  = subject_get_int(cfg_cur.filter.low);
    int32_t      high = subject_get_int(cfg_cur.filter.high);

    /* Nivel de la respuesta (meseta): con headroom arriba, para que se lea como
     * respuesta esquematica y no como una meseta maciza de borde a borde. */
    int32_t pass_y = top + (base - top) / 4;

    /* UNA sola regla de orientacion (issue #182): el mismo helper dependiente del
     * modo que usan el hit-test, el inverso del arrastre y las etiquetas. En LSB
     * el eje se espeja (0 a la derecha, HIGH a la izquierda) y no se asume
     * low_x < high_x: el relleno, la meseta y los faldones usan band_left/right. */
    dfl_graph_layout_t L = dfl_graph_layout(mode, low, high, d, px0, pw);

    int32_t band_l = L.band_left;
    int32_t band_r = L.band_right;
    int32_t hl     = L.low_x;
    int32_t hh     = L.high_x;

    /* Relleno tematico sutil de la banda pasante (no una meseta maciza). */
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color = TC(vfo_line);
    rect.bg_opa   = LV_OPA_20;
    rect.radius   = 0;

    lv_area_t area;
    area.x1 = band_l;
    area.y1 = pass_y;
    area.x2 = band_r;
    area.y2 = base;
    lv_draw_rect(draw_ctx, &rect, &area);

    /* Linea base y referencia del cero (ejes ligeros, no cuadricula). */
    lv_draw_line_dsc_init(&line);
    line.color = TC(line);
    line.width = 1;

    lv_point_t a;
    lv_point_t b;
    a.x = px0;
    a.y = base;
    b.x = px0 + pw;
    b.y = base;
    lv_draw_line(draw_ctx, &line, &a, &b);

    int32_t zero_x = L.zero_x;
    a.x = zero_x;
    a.y = top;
    b.x = zero_x;
    b.y = base;
    lv_draw_line(draw_ctx, &line, &a, &b);

    /* Respuesta esquematica: meseta entre los bordes y flancos a la baseline. */
    line.color = TC(accent);
    line.width = 2;

    lv_point_t pts[6];
    int        n = 0;
    int32_t    left_skirt  = band_l - GRAPH_SKIRT;
    int32_t    right_skirt = band_r + GRAPH_SKIRT;

    if (left_skirt < px0) {
        left_skirt = px0;
    }
    if (right_skirt > px0 + pw) {
        right_skirt = px0 + pw;
    }

    pts[n].x = px0;         pts[n].y = base;   n++;
    pts[n].x = left_skirt;  pts[n].y = base;   n++;
    pts[n].x = band_l;      pts[n].y = pass_y; n++;
    pts[n].x = band_r;      pts[n].y = pass_y; n++;
    pts[n].x = right_skirt; pts[n].y = base;   n++;
    pts[n].x = px0 + pw;    pts[n].y = base;   n++;

    for (int i = 1; i < n; i++) {
        lv_draw_line(draw_ctx, &line, &pts[i - 1], &pts[i]);
    }

    /* Handles visuales de 8 px (zona de toque >=44x44 por el hit-test de 22 px).
     * El de LOW se ve apagado cuando no es editable. */
    lv_draw_rect_dsc_init(&rect);
    rect.radius = 3;
    rect.bg_opa = LV_OPA_COVER;

    rect.bg_color = (dfl_mode_low_editable(mode) && !tx_active) ? TC(accent) : TC(text_mute);
    area.x1 = hl - 4;
    area.x2 = hl + 3;
    area.y1 = top;
    area.y2 = base;
    lv_draw_rect(draw_ctx, &rect, &area);

    rect.bg_color = tx_active ? TC(text_mute) : TC(accent);
    area.x1 = hh - 4;
    area.x2 = hh + 3;
    area.y1 = top;
    area.y2 = base;
    lv_draw_rect(draw_ctx, &rect, &area);

    /* Ticks del dominio (2-4 marcas) + cero visible, en el rol `text_dim`. */
    lv_draw_label_dsc_t lbl;

    lv_draw_label_dsc_init(&lbl);
    lbl.color = TC(text_dim);
    lbl.font  = &sony_14;

    char       txt[16];
    lv_point_t sz;

    /* Extremos del eje en el orden que muestra el modo: USB `0` izq / `M` der;
     * LSB `M` izq / `0` der. Siempre magnitudes positivas en Hz (nunca MHz ni
     * frecuencias RF negativas). */
    snprintf(txt, sizeof(txt), "%d", L.left_hz);
    lv_txt_get_size(&sz, txt, lbl.font, 0, 0, LV_COORD_MAX, 0);
    area.x1 = px0;
    area.y1 = base + 2;
    area.x2 = area.x1 + sz.x;
    area.y2 = area.y1 + sz.y;
    lv_draw_label(draw_ctx, &lbl, &area, txt, NULL);

    snprintf(txt, sizeof(txt), "%d", L.right_hz);
    lv_txt_get_size(&sz, txt, lbl.font, 0, 0, LV_COORD_MAX, 0);
    area.x1 = px0 + pw - sz.x;
    area.y1 = base + 2;
    area.x2 = area.x1 + sz.x;
    area.y2 = area.y1 + sz.y;
    lv_draw_label(draw_ctx, &lbl, &area, txt, NULL);
}

/* -------------------------------------------------------------------------- */
/* Interaccion del grafico (hit-test <=22 px; un toque lejano no edita nada). */

static void drag_apply(lv_obj_t *obj, int32_t local_x) {
    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    dfl_domain_t d    = current_domain(mode);
    int32_t      pw   = lv_obj_get_width(obj) - GRAPH_PAD_L - GRAPH_PAD_R;

    /* Inverso dependiente del modo (issue #182): en LSB des-espeja antes de
     * convertir a Hz, asi mover HIGH a la derecha lo acerca a cero (HIGH
     * disminuye) y mover LOW a la derecha tambien lo disminuye. En USB el
     * inverso actual no cambia. */
    int32_t hz = dfl_graph_mode_hz(mode, local_x, d, GRAPH_PAD_L, pw);

    if (dragging == DFL_HANDLE_LOW) {
        apply_low(dfl_quantize(hz, dfl_step_low(mode)));
    } else if (dragging == DFL_HANDLE_HIGH) {
        apply_high(dfl_quantize(hz, dfl_step_high(mode)));
    }
}

static void graph_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t       *obj  = lv_event_get_target(e);
    lv_point_t      p;

    switch (code) {
        case LV_EVENT_PRESSED: {
            if (tx_active) {
                dragging = DFL_HANDLE_NONE;
                break;
            }
            if (!pointer_get(&p)) {
                dragging = DFL_HANDLE_NONE;
                break;
            }

            x6200_mode_t mode   = subject_get_int(cfg_cur.mode);
            dfl_domain_t d      = current_domain(mode);
            int32_t      pw     = lv_obj_get_width(obj) - GRAPH_PAD_L - GRAPH_PAD_R;
            int32_t      low    = subject_get_int(cfg_cur.filter.low);
            int32_t      high   = subject_get_int(cfg_cur.filter.high);

            /* Mismas coordenadas dependientes del modo que draw_cb (issue #182):
             * en LSB el hit-test usa las x espejadas, asi tocar el handle que
             * esta a la izquierda captura HIGH (no LOW). */
            dfl_graph_layout_t L = dfl_graph_layout(mode, low, high, d, GRAPH_PAD_L, pw);

            dragging = dfl_hit_test(p.x - obj->coords.x1, L.low_x, L.high_x);

            if (dragging == DFL_HANDLE_LOW && !dfl_mode_low_editable(mode)) {
                dragging = DFL_HANDLE_HIGH;
            }

            /* Arrastrar un handle SELECCIONA ese parametro: asi la rueda sigue
             * con el que el usuario acaba de tocar. Un toque lejano no cambia
             * nada (ni seleccion ni filtro). */
            if (dragging == DFL_HANDLE_LOW) {
                dfl_inline_select(DFL_SEL_LOW);
            } else if (dragging == DFL_HANDLE_HIGH) {
                dfl_inline_select(DFL_SEL_HIGH);
            }
            break;
        }

        case LV_EVENT_PRESSING:
            if (tx_active || dragging == DFL_HANDLE_NONE) {
                break;
            }
            if (pointer_get(&p)) {
                drag_apply(obj, p.x - obj->coords.x1);
            }
            break;

        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST:
            dragging = DFL_HANDLE_NONE;
            break;

        default:
            break;
    }
}

/* Una fila tocada selecciona su parametro. */
static dfl_sel_t row_sel_of(dfl_inline_ui_id_t id) {
    switch (id) {
        case DFL_UI_LOW_ROW:
            return DFL_SEL_LOW;
        case DFL_UI_HIGH_ROW:
            return DFL_SEL_HIGH;
        default:
            return DFL_SEL_BW;
    }
}

static void row_cb(lv_event_t *e) {
    lv_obj_t            *obj = lv_event_get_target(e);
    dfl_inline_ui_id_t   id  = (dfl_inline_ui_id_t)(intptr_t) lv_obj_get_user_data(obj);

    if (tx_active) {
        return;
    }

    dfl_inline_select(row_sel_of(id));
}

static void reset_cb(lv_event_t *e) {
    (void) e;

    reset_default();
}

/* -------------------------------------------------------------------------- */
/* Refresh: TODO sale de los Subjects, nunca de una posicion optimista. */

static void row_refresh(dfl_sel_t s, int32_t value, bool editable) {
    lv_obj_t *r = row[s];

    if (!r) {
        return;
    }

    bool       selected = (s == sel) && !tx_active;
    lv_color_t fg       = editable ? TC(text_value) : TC(text_mute);
    lv_color_t name_fg  = TC(text_dim);

    lv_label_set_text_fmt(row_value[s], "%d Hz", (int) value);

    if (selected) {
        lv_obj_set_style_bg_color(r, TC(sel_bg), 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(r, TC(sel_border), 0);
        fg      = TC(sel_text);
        name_fg = TC(sel_text);
        lv_obj_clear_flag(row_bar[s], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(r, TC(panel_border), 0);
        lv_obj_add_flag(row_bar[s], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_style_bg_color(row_bar[s], selected ? TC(sel_text) : TC(accent), 0);
    lv_obj_set_style_text_color(row_name[s], name_fg, 0);
    lv_obj_set_style_text_color(row_value[s], fg, 0);
}

static void dfl_refresh(void) {
    if (!graph || !panel) {
        return;
    }

    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    int32_t      low  = subject_get_int(cfg_cur.filter.low);
    int32_t      high = subject_get_int(cfg_cur.filter.high);
    int32_t      bw   = subject_get_int(cfg_cur.filter.bw);

    /* Histeresis de ampliacion: el dominio fijado solo crece si el filtro se
     * fue afuera; nunca se reduce automaticamente. */
    int32_t grown = dfl_domain_grow(domain_span, low, high);

    if (grown != domain_span) {
        domain_span = grown;
    }

    if (mode_label) {
        lv_label_set_text_fmt(mode_label, "DFL %s", info_params_mode_label_get());
    }

    row_refresh(DFL_SEL_LOW, low, dfl_mode_low_editable(mode) && !tx_active);
    row_refresh(DFL_SEL_HIGH, high, !tx_active);
    row_refresh(DFL_SEL_BW, bw, !tx_active);

    if (reset_btn) {
        if (tx_active) {
            lv_obj_add_state(reset_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(reset_btn, LV_STATE_DISABLED);
        }
    }

    lv_obj_invalidate(graph);
}

static void on_subject_change(Subject *subj, void *user_data) {
    (void) subj;
    (void) user_data;

    if (expanded) {
        dfl_refresh();
    }
}

/* Al cambiar de modo se re-fija el dominio (C.3) y se CANCELA cualquier drag
 * activo (issue #182): el cambio USB<->LSB espeja el eje en el mismo frame, asi
 * que no se pueden reusar las coordenadas/handles del modo anterior. */
static void on_mode_change(Subject *subj, void *user_data) {
    (void) subj;
    (void) user_data;

    if (expanded) {
        x6200_mode_t mode = subject_get_int(cfg_cur.mode);

        dragging    = DFL_HANDLE_NONE;
        domain_span = dfl_fit_span(subject_get_int(cfg_cur.filter.low),
                                   subject_get_int(cfg_cur.filter.high));
        (void) mode;
        dfl_refresh();
    }
}

/* -------------------------------------------------------------------------- */
/* Construccion */

static lv_obj_t *make_reset_button(lv_obj_t *parent) {
    lv_obj_t *b = lv_obj_create(parent);

    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 128, DFL_BTN);
    lv_obj_set_pos(b, DFL_LEFT_X + DFL_LEFT_W - 128, DFL_HEAD_Y);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_bg_color(b, TC(chip), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, TC(panel_border), 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(b, (void *)(intptr_t) DFL_UI_RESET);
    lv_obj_add_event_cb(b, reset_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = lv_label_create(b);

    lv_obj_set_style_text_font(l, &sony_18, 0);
    lv_obj_set_style_text_color(l, TC(text), 0);
    lv_label_set_text(l, "Reset");
    lv_obj_center(l);

    return b;
}

static void make_row(lv_obj_t *parent, int32_t y, const char *name, dfl_sel_t s, dfl_inline_ui_id_t id) {
    lv_obj_t *r = lv_obj_create(parent);

    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, DFL_RIGHT_W, DFL_ROW_H);
    lv_obj_set_pos(r, DFL_RIGHT_X, y);
    lv_obj_set_style_radius(r, 4, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(r, TC(panel_border), 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(r, (void *)(intptr_t) id);
    lv_obj_add_event_cb(r, row_cb, LV_EVENT_CLICKED, NULL);

    /* Barra lateral corta: marca de foco que no depende solo del color. */
    lv_obj_t *bar = lv_obj_create(r);

    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 4, 28);
    lv_obj_set_pos(bar, 3, 8);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_bg_color(bar, TC(accent), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *name_l = make_label(r, 14, 13, &sony_18, TC(text_dim), 100);

    lv_label_set_text(name_l, name);

    lv_obj_t *value = make_label(r, 110, 9, &sony_22, TC(text_value), 168);

    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(value, "0 Hz");

    row[s]       = r;
    row_bar[s]   = bar;
    row_name[s]  = name_l;
    row_value[s] = value;
}

void dfl_inline_init(lv_obj_t *waterfall_widget) {
    if (panel != NULL || waterfall_widget == NULL) {
        return;
    }

    /* Coordenadas reales del waterfall y de la barra de botones: NO se
     * hardcodea 240. El panel es hermano del waterfall (hijo de la pantalla) para
     * no alterar el invariante de la linea central (issue #140) y termina donde
     * empieza la barra inferior. */
    lv_obj_t *parent = lv_obj_get_parent(waterfall_widget);

    lv_obj_update_layout(waterfall_widget);

    lv_area_t wa;

    lv_obj_get_coords(waterfall_widget, &wa);

    lv_coord_t wf_w = wa.x2 - wa.x1 + 1;
    lv_coord_t wf_h = wa.y2 - wa.y1 + 1;
    lv_coord_t p_h  = wf_h - DFL_MICRO_WF - (lv_coord_t) buttons_bar_height();

    if (p_h < DFL_PANEL_MIN_H) {
        /* Perfil sin alto suficiente: se conserva el minimo tactil y el
         * waterfall vivo; el arnes de UI corta si el layout no entra. */
        p_h = wf_h - DFL_MICRO_WF;
    }

    panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, wa.x1, wa.y1 + DFL_MICRO_WF);
    lv_obj_set_size(panel, wf_w, p_h);
    lv_obj_set_style_bg_color(panel, TC(panel), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);

    /* Separador discreto entre zona grafica y zona de seleccion. */
    lv_obj_t *sep = lv_obj_create(panel);
    lv_obj_remove_style_all(sep);
    lv_obj_set_pos(sep, DFL_SEP_X, DFL_HEAD_Y);
    lv_obj_set_size(sep, 1, p_h - 2 * DFL_HEAD_Y);
    lv_obj_set_style_bg_color(sep, TC(panel_border), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);

    mode_label = make_label(panel, DFL_LEFT_X, DFL_HEAD_Y + 12, &sony_18, TC(text_head), 320);
    lv_label_set_text(mode_label, "DFL");

    reset_btn = make_reset_button(panel);

    graph = lv_obj_create(panel);
    lv_obj_remove_style_all(graph);
    lv_obj_set_size(graph, DFL_GRAPH_W, DFL_GRAPH_H);
    lv_obj_set_pos(graph, DFL_GRAPH_X, DFL_GRAPH_Y);
    lv_obj_set_style_bg_color(graph, TC(panel), 0);
    lv_obj_set_style_bg_opa(graph, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(graph, 1, 0);
    lv_obj_set_style_border_color(graph, TC(panel_border), 0);
    lv_obj_set_style_radius(graph, 3, 0);
    lv_obj_set_style_pad_all(graph, 0, 0);
    lv_obj_add_flag(graph, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(graph, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(graph, (void *)(intptr_t) DFL_UI_GRAPH);
    lv_obj_add_event_cb(graph, draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(graph, graph_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(graph, graph_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(graph, graph_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(graph, graph_event_cb, LV_EVENT_PRESS_LOST, NULL);

    /* Marca de respuesta esquematica (no simula medicion DSP). */
    lv_obj_t *esq = make_label(graph, DFL_GRAPH_W - 78, 54, &sony_14, TC(text_dim), 72);

    lv_obj_set_style_text_align(esq, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(esq, "Schematic");

    make_row(panel, DFL_ROW_Y(0), "LOW", DFL_SEL_LOW, DFL_UI_LOW_ROW);
    make_row(panel, DFL_ROW_Y(1), "HIGH", DFL_SEL_HIGH, DFL_UI_HIGH_ROW);
    make_row(panel, DFL_ROW_Y(2), "BW", DFL_SEL_BW, DFL_UI_BW_ROW);

    tx_active = (radio_get_state() != RADIO_RX);

    sel         = dfl_default_sel(subject_get_int(cfg_cur.mode));
    domain_span = dfl_fit_span(subject_get_int(cfg_cur.filter.low),
                               subject_get_int(cfg_cur.filter.high));

    obs_mode = subject_add_delayed_observer(cfg_cur.mode, on_mode_change, NULL);
    obs_low  = subject_add_delayed_observer(cfg_cur.filter.low, on_subject_change, NULL);
    obs_high = subject_add_delayed_observer(cfg_cur.filter.high, on_subject_change, NULL);
    obs_bw   = subject_add_delayed_observer(cfg_cur.filter.bw, on_subject_change, NULL);

    dfl_refresh();
}

void dfl_inline_shutdown(void) {
    if (obs_mode) {
        observer_delayed_del(obs_mode);
        obs_mode = NULL;
    }
    if (obs_low) {
        observer_delayed_del(obs_low);
        obs_low = NULL;
    }
    if (obs_high) {
        observer_delayed_del(obs_high);
        obs_high = NULL;
    }
    if (obs_bw) {
        observer_delayed_del(obs_bw);
        obs_bw = NULL;
    }

    dragging    = DFL_HANDLE_NONE;
    expanded    = false;
    panel       = NULL;
    graph       = NULL;
    mode_label  = NULL;
    reset_btn   = NULL;

    for (int i = 0; i < DFL_SEL_COUNT; i++) {
        row[i]       = NULL;
        row_bar[i]   = NULL;
        row_name[i]  = NULL;
        row_value[i] = NULL;
    }
}

/* -------------------------------------------------------------------------- */
/* Expandir / colapsar (con guardado/restauracion del encoder VOL). */

void dfl_inline_select(dfl_sel_t s) {
    if (s < 0 || s >= DFL_SEL_COUNT) {
        return;
    }
    if (s == DFL_SEL_LOW && !dfl_mode_low_editable(subject_get_int(cfg_cur.mode))) {
        /* LOW no es editable en AM/SAM/NFM/WFM: no se selecciona. */
        return;
    }

    sel = s;
    buttons_dfl_expanded_sync((int) sel);
    dfl_refresh();
}

dfl_sel_t dfl_inline_get_sel(void) {
    return sel;
}

void dfl_inline_expand(void) {
    if (panel == NULL || expanded) {
        return;
    }

    /* Guardar el estado del encoder VOL para restaurarlo al colapsar. */
    saved_vol_mode = vol_get_mode();
    saved_rot_mode = (vol != NULL) ? (int) vol->mode : -1;

    /* Entrar en EDICION: la rueda ajusta el parametro seleccionado de inmediato.
     * El volumen no cambia: vol_update() no se llama mientras expandido. */
    if (vol != NULL) {
        vol->mode = ROT_VOL_EDIT_MODE;
    }

    sel         = dfl_default_sel(subject_get_int(cfg_cur.mode));
    domain_span = dfl_fit_span(subject_get_int(cfg_cur.filter.low),
                               subject_get_int(cfg_cur.filter.high));

    expanded = true;

    lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);

    buttons_dfl_graph_open();
    buttons_dfl_expanded_sync((int) sel);

    dfl_refresh();
}

void dfl_inline_collapse(void) {
    if (!expanded) {
        return;
    }

    expanded    = false;
    dragging    = DFL_HANDLE_NONE;

    if (panel) {
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }

    /* Restaurar el encoder VOL (modo/control previo). */
    if (vol != NULL && saved_rot_mode >= 0) {
        vol->mode = (uint8_t) saved_rot_mode;
    }
    if (vol != NULL) {
        vol_set_mode(saved_vol_mode);
    }

    buttons_load_page(&buttons_page_dfl_1);
}

void dfl_inline_toggle(void) {
    if (expanded) {
        dfl_inline_collapse();
    } else {
        dfl_inline_expand();
    }
}

bool dfl_inline_is_expanded(void) {
    return expanded;
}

/* -------------------------------------------------------------------------- */
/* Ruteo del encoder VOL (rotary1). NO toca frecuencia ni volumen. */

static void cycle_sel(int dir) {
    x6200_mode_t mode   = subject_get_int(cfg_cur.mode);
    bool         low_ok = dfl_mode_low_editable(mode);
    int          n      = low_ok ? 3 : 2;
    dfl_sel_t    order[3];
    int          idx = 0;

    if (low_ok) {
        order[idx++] = DFL_SEL_LOW;
    }
    order[idx++] = DFL_SEL_HIGH;
    order[idx++] = DFL_SEL_BW;

    int cur = 0;

    for (int i = 0; i < n; i++) {
        if (order[i] == sel) {
            cur = i;
            break;
        }
    }

    int next = (cur + (dir > 0 ? 1 : n - 1)) % n;

    sel = order[next];
    buttons_dfl_expanded_sync((int) sel);
    dfl_refresh();
}

bool dfl_inline_on_vol_key(uint32_t key) {
    if (!expanded) {
        return false;
    }

    switch (key) {
        case KEY_VOL_LEFT_EDIT:
            if (!tx_active) {
                dfl_rotary_apply(sel, -1);
                dfl_refresh();
            }
            return true;

        case KEY_VOL_RIGHT_EDIT:
            if (!tx_active) {
                dfl_rotary_apply(sel, +1);
                dfl_refresh();
            }
            return true;

        case KEY_VOL_LEFT_SELECT:
            if (!tx_active) {
                cycle_sel(-1);
            }
            return true;

        case KEY_VOL_RIGHT_SELECT:
            if (!tx_active) {
                cycle_sel(+1);
            }
            return true;

        default:
            return false;
    }
}

void dfl_inline_on_radio_event(lv_event_code_t code) {
    tx_active = (code == EVENT_RADIO_TX);

    if (expanded) {
        dfl_refresh();
    }
}

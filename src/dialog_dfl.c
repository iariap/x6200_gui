/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Editor DFL (issue #173, PR 2).
 *
 *  Dialogo modal compacto sobre el waterfall: encabezado "DFL - Digital Filter"
 *  + modo actual + X, un grafico ESQUEMATICO de banda pasante (no una medicion
 *  DSP) con dos handles, y los controles LOW / HIGH / BW con su valor entero
 *  exacto en Hz. Cada fila es TOCABLE y queda resaltada al seleccionarse; la
 *  RUEDA (encoder) ajusta el parametro seleccionado y el arrastre del grafico
 *  sigue como atajo (arrastrar un handle tambien lo selecciona). Restablece los
 *  predeterminados del modo con mode_default_values() de src/cfg/mode.c,
 *  actualizando solo LOW/HIGH.
 *
 *  El dialogo NO es una app: no guarda ni restaura el estado de la radio
 *  (restore_radio = false). Los pasos, limites, geometria y BW viven en
 *  src/dfl_model.h (puro, con test nativo) y la aplicacion por los setters
 *  vigentes en src/dfl_rotary.c (tambien con test nativo).
 */

#include "dialog_dfl.h"

#include "dfl_model.h"
#include "dfl_rotary.h"

#include "buttons.h"
#include "cfg/cfg.h"
#include "cfg/mode.h"
#include "cfg/mode.private.h"
#include "events.h"
#include "info.h"
#include "keyboard.h"
#include "radio.h"
#include "styles.h"
#include "theme.h"

#include <stdio.h>

/* Bounding box del contrato (§4): x=40, y=202, w=720, h=206. */
#define DFL_W            720
#define DFL_H            206
#define DFL_X            40
#define DFL_Y            202

/* Touch target minimo del contrato de calidad (44 px). */
#define DFL_BTN          44

/* Fila de control (etiqueta + valor) tocable: x/ancho dentro del dialogo. */
#define DFL_ROW_X        452
#define DFL_ROW_W        260
#define DFL_ROW_H        44

/* Margenes del area de dibujo del grafico. */
#define GRAPH_PAD_L      6
#define GRAPH_PAD_R      6
#define GRAPH_PAD_T      6
#define GRAPH_AXIS_H     20
#define GRAPH_SKIRT      14

static dialog_t  dialog;

static lv_obj_t *graph;
static lv_obj_t *mode_label;
static lv_obj_t *tx_label;
static lv_obj_t *row[DFL_SEL_COUNT];
static lv_obj_t *row_name[DFL_SEL_COUNT];
static lv_obj_t *row_value[DFL_SEL_COUNT];
static lv_obj_t *btn[DFL_UI_CLOSE + 1];

static ObserverDelayed *obs_mode;
static ObserverDelayed *obs_low;
static ObserverDelayed *obs_high;
static ObserverDelayed *obs_bw;

static bool         tx_active;
static dfl_sel_t    sel = DFL_SEL_LOW;
static dfl_handle_t dragging = DFL_HANDLE_NONE;

/* Seam de test: en la radio queda NULL y el arrastre lee el indev de LVGL. */
bool (*dialog_dfl_pointer_hook)(lv_point_t *out) = NULL;

/* -------------------------------------------------------------------------- */

static bool dfl_pointer_get(lv_point_t *out) {
    if (dialog_dfl_pointer_hook) {
        return dialog_dfl_pointer_hook(out);
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

static void set_btn_enabled(lv_obj_t *b, bool on) {
    if (!b) {
        return;
    }

    lv_obj_t *label = lv_obj_get_child(b, 0);

    if (on) {
        lv_obj_clear_state(b, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        if (label) {
            lv_obj_set_style_text_color(label, TC(sel_text), 0);
        }
    } else {
        lv_obj_add_state(b, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(b, LV_OPA_30, 0);
        if (label) {
            lv_obj_set_style_text_color(label, TC(text_mute), 0);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Aplicaciones: siempre por los setters/caminos vigentes del firmware, via
 * src/dfl_rotary.c (que nunca devuelve un valor optimista: relee el Subject). */

static void dfl_refresh(void);

static void dfl_apply_low(int32_t val) {
    if (tx_active) {
        return;
    }

    dfl_set_low(val);
    dfl_refresh();
}

static void dfl_apply_high(int32_t val) {
    if (tx_active) {
        return;
    }

    dfl_set_high(val);
    dfl_refresh();
}

/* BW: se valida ANTES de escribir (ver src/dfl_rotary.c). Si el par LOW/HIGH
 * resultante no es legal se bloquea la accion y se conserva el estado; el label
 * sigue leyendo el Subject, asi que no queda un valor optimista. */
static void dfl_apply_bw(int32_t val) {
    if (tx_active) {
        return;
    }

    dfl_set_bw(val);
    dfl_refresh();
}

/* La rueda (encoder) ajusta el parametro SELECCIONADO con los pasos reales de
 * src/vol.c. Durante TX no edita (el dialogo se ve deshabilitado). El mensaje y
 * el valor salen del Subject, nunca de una posicion optimista. */
static void rotary_cb(int32_t diff) {
    if (tx_active || diff == 0) {
        return;
    }

    dfl_rotary_apply(sel, diff);
    dfl_refresh();
}

static void dfl_reset_default(void) {
    if (tx_active) {
        return;
    }

    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    int32_t      def_low;
    int32_t      def_high;
    uint32_t     step;
    uint32_t     zoom;

    /* Confirmar los valores originales del modo antes de escribir: si la tabla
     * no cubre el modo, no se toca nada. */
    if (!mode_default_values(xmode_2_db(mode), &def_low, &def_high, &step, &zoom)) {
        return;
    }

    int32_t cur_low  = subject_get_int(cfg_cur.filter.low);
    int32_t cur_high = subject_get_int(cfg_cur.filter.high);

    /* Los setters tienen guardas (high > low, low < high): el orden que deja
     * pasar las dos escrituras depende del estado actual. */
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
/* Grafico esquematico */

static void draw_cb(lv_event_t *e) {
    lv_obj_t          *obj      = lv_event_get_target(e);
    lv_draw_ctx_t     *draw_ctx = lv_event_get_draw_ctx(e);
    lv_draw_line_dsc_t line;
    lv_draw_rect_dsc_t rect;

    lv_coord_t x1 = obj->coords.x1;
    lv_coord_t y1 = obj->coords.y1;
    lv_coord_t w  = lv_obj_get_width(obj);
    lv_coord_t h  = lv_obj_get_height(obj);

    int32_t px0 = x1 + GRAPH_PAD_L;
    int32_t pw  = w - GRAPH_PAD_L - GRAPH_PAD_R;
    int32_t top = y1 + GRAPH_PAD_T;
    int32_t base = y1 + h - GRAPH_AXIS_H;

    if (pw <= 0 || base <= top) {
        return;
    }

    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    dfl_domain_t d    = dfl_graph_domain(mode);
    int32_t      low  = subject_get_int(cfg_cur.filter.low);
    int32_t      high = subject_get_int(cfg_cur.filter.high);

    int32_t band_l;
    int32_t band_r;
    int32_t hl;
    int32_t hh;

    if (dfl_mode_is_symmetric(mode)) {
        band_l = dfl_graph_x(-high, d, px0, pw);
        band_r = dfl_graph_x(high, d, px0, pw);
        hl     = dfl_graph_x(0, d, px0, pw);
        hh     = dfl_graph_x(high, d, px0, pw);
    } else {
        band_l = dfl_graph_x(low, d, px0, pw);
        band_r = dfl_graph_x(high, d, px0, pw);
        hl     = band_l;
        hh     = band_r;
    }

    /* Relleno de la banda pasante (discreto). */
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color = TC(vfo_line);
    rect.bg_opa   = LV_OPA_40;
    rect.radius   = 0;

    lv_area_t area;
    area.x1 = band_l;
    area.y1 = top;
    area.x2 = band_r;
    area.y2 = base;
    lv_draw_rect(draw_ctx, &rect, &area);

    /* Ejes: baseline y referencia del cero. */
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

    int32_t zero_x = dfl_graph_x(0, d, px0, pw);
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

    pts[n].x = px0;         pts[n].y = base; n++;
    pts[n].x = left_skirt;  pts[n].y = base; n++;
    pts[n].x = band_l;      pts[n].y = top;  n++;
    pts[n].x = band_r;      pts[n].y = top;  n++;
    pts[n].x = right_skirt; pts[n].y = base; n++;
    pts[n].x = px0 + pw;    pts[n].y = base; n++;

    for (int i = 1; i < n; i++) {
        lv_draw_line(draw_ctx, &line, &pts[i - 1], &pts[i]);
    }

    /* Handles grandes. El de LOW se muestra apagado cuando no es editable. */
    lv_draw_rect_dsc_init(&rect);
    rect.radius = 3;
    rect.bg_opa = LV_OPA_COVER;

    rect.bg_color = dfl_mode_low_editable(mode) ? TC(accent) : TC(text_mute);
    area.x1 = hl - 4;
    area.x2 = hl + 3;
    area.y1 = top;
    area.y2 = base;
    lv_draw_rect(draw_ctx, &rect, &area);

    rect.bg_color = TC(accent);
    area.x1 = hh - 4;
    area.x2 = hh + 3;
    area.y1 = top;
    area.y2 = base;
    lv_draw_rect(draw_ctx, &rect, &area);

    /* Etiquetas de eje (decorativas; no son objetos). */
    lv_draw_label_dsc_t lbl;

    lv_draw_label_dsc_init(&lbl);
    lbl.color = TC(text_dim);
    lbl.font  = &sony_14;

    char txt[16];
    lv_point_t sz;

    snprintf(txt, sizeof(txt), "%d", d.min_hz);
    lv_txt_get_size(&sz, txt, lbl.font, 0, 0, LV_COORD_MAX, 0);
    area.x1 = px0;
    area.y1 = base + 2;
    area.x2 = area.x1 + sz.x;
    area.y2 = area.y1 + sz.y;
    lv_draw_label(draw_ctx, &lbl, &area, txt, NULL);

    snprintf(txt, sizeof(txt), "%d", d.max_hz);
    lv_txt_get_size(&sz, txt, lbl.font, 0, 0, LV_COORD_MAX, 0);
    area.x1 = px0 + pw - sz.x;
    area.y1 = base + 2;
    area.x2 = area.x1 + sz.x;
    area.y2 = area.y1 + sz.y;
    lv_draw_label(draw_ctx, &lbl, &area, txt, NULL);
}

/* -------------------------------------------------------------------------- */
/* Interaccion */

static void dfl_drag_apply(lv_obj_t *obj, int32_t local_x) {
    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    dfl_domain_t d    = dfl_graph_domain(mode);
    int32_t      pw   = lv_obj_get_width(obj) - GRAPH_PAD_L - GRAPH_PAD_R;
    int32_t      hz   = dfl_graph_hz(local_x, d, GRAPH_PAD_L, pw);

    if (dragging == DFL_HANDLE_LOW) {
        dfl_apply_low(dfl_quantize(hz, dfl_step_low(mode)));
    } else if (dragging == DFL_HANDLE_HIGH) {
        dfl_apply_high(dfl_quantize(hz, dfl_step_high(mode)));
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
            if (!dfl_pointer_get(&p)) {
                dragging = DFL_HANDLE_NONE;
                break;
            }

            /* La posicion de los handles se recalcula aca (no se cachea): en el
             * firmware el primer arrastre puede llegar antes de que el layout
             * haya fijado las coordenadas del grafico. */
            x6200_mode_t mode   = subject_get_int(cfg_cur.mode);
            dfl_domain_t d      = dfl_graph_domain(mode);
            int32_t      pw     = lv_obj_get_width(obj) - GRAPH_PAD_L - GRAPH_PAD_R;
            int32_t      low    = subject_get_int(cfg_cur.filter.low);
            int32_t      high   = subject_get_int(cfg_cur.filter.high);
            int32_t      low_x  = dfl_graph_x(low, d, GRAPH_PAD_L, pw);
            int32_t      high_x = dfl_graph_x(high, d, GRAPH_PAD_L, pw);

            dragging = dfl_nearest_handle(p.x - obj->coords.x1, low_x, high_x);

            if (dragging == DFL_HANDLE_LOW && !dfl_mode_low_editable(mode)) {
                dragging = DFL_HANDLE_HIGH;
            }

            /* Arrastrar un handle SELECCIONA ese parametro: asi la rueda sigue
             * con el que el usuario acaba de tocar. */
            if (dragging == DFL_HANDLE_LOW) {
                sel = DFL_SEL_LOW;
            } else if (dragging == DFL_HANDLE_HIGH) {
                sel = DFL_SEL_HIGH;
            }
            dfl_refresh();
            break;
        }

        case LV_EVENT_PRESSING:
            if (tx_active || dragging == DFL_HANDLE_NONE) {
                break;
            }
            if (dfl_pointer_get(&p)) {
                dfl_drag_apply(obj, p.x - obj->coords.x1);
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

/* Una fila tocada selecciona su parametro: queda resaltada (roles del tema) y
 * la rueda pasa a ajustarla. */
static dfl_sel_t row_sel_of(dfl_ui_id_t id) {
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
    lv_obj_t   *obj = lv_event_get_target(e);
    dfl_ui_id_t id  = (dfl_ui_id_t)(intptr_t)lv_obj_get_user_data(obj);

    if (tx_active) {
        return;
    }

    sel = row_sel_of(id);
    dfl_refresh();
}

static void button_cb(lv_event_t *e) {
    lv_obj_t   *obj = lv_event_get_target(e);
    dfl_ui_id_t id  = (dfl_ui_id_t)(intptr_t)lv_obj_get_user_data(obj);

    if (tx_active && id != DFL_UI_CLOSE) {
        return;
    }

    switch (id) {
        case DFL_UI_RESET:
            dfl_reset_default();
            break;
        case DFL_UI_CLOSE:
            dialog_destruct();
            break;
        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* Refresh: TODO sale de los Subjects, nunca de una posicion optimista. */

/* Fila: valor del Subject + resaltado del tema cuando esta seleccionada. La
 * rueda edita la seleccionada; en TX ninguna queda resaltada y el valor va
 * atenuado (deshabilitado visible). */
static void dfl_row_refresh(dfl_sel_t s, int32_t value, bool editable) {
    lv_obj_t *r = row[s];

    if (!r) {
        return;
    }

    bool       selected = (s == sel) && !tx_active;
    lv_color_t fg       = editable ? TC(text) : TC(text_mute);
    lv_color_t name_fg  = TC(text_dim);

    lv_label_set_text_fmt(row_value[s], "%d Hz", (int) value);

    if (selected) {
        lv_obj_set_style_bg_color(r, TC(sel_bg), 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(r, TC(sel_border), 0);
        fg      = TC(sel_text);
        name_fg = TC(sel_text);
    } else {
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(r, TC(popup_border), 0);
    }

    lv_obj_set_style_text_color(row_name[s], name_fg, 0);
    lv_obj_set_style_text_color(row_value[s], fg, 0);
}

static void dfl_refresh(void) {
    if (!graph || !row[DFL_SEL_LOW]) {
        return;
    }

    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    int32_t      low  = subject_get_int(cfg_cur.filter.low);
    int32_t      high = subject_get_int(cfg_cur.filter.high);
    int32_t      bw   = subject_get_int(cfg_cur.filter.bw);

    lv_label_set_text(mode_label, info_params_mode_label_get());
    lv_label_set_text(tx_label, tx_active ? "TX" : "");

    dfl_row_refresh(DFL_SEL_LOW, low, dfl_mode_low_editable(mode) && !tx_active);
    dfl_row_refresh(DFL_SEL_HIGH, high, !tx_active);
    dfl_row_refresh(DFL_SEL_BW, bw, !tx_active);

    set_btn_enabled(btn[DFL_UI_RESET], !tx_active);

    lv_obj_invalidate(graph);
}

static void on_subject_change(Subject *subj, void *user_data) {
    (void) subj;
    (void) user_data;
    dfl_refresh();
}

static void tx_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    tx_active = (code == EVENT_RADIO_TX) ? true : false;
    dfl_refresh();
}

/* -------------------------------------------------------------------------- */
/* Construccion / destruccion */

static lv_obj_t *make_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                             const char *txt, dfl_ui_id_t id) {
    lv_obj_t *b = lv_obj_create(parent);

    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_bg_color(b, TC(sel_bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, TC(sel_border), 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(b, (void *)(intptr_t) id);
    lv_obj_add_event_cb(b, button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = lv_label_create(b);

    lv_obj_set_style_text_font(l, &sony_20, 0);
    lv_obj_set_style_text_color(l, TC(sel_text), 0);
    lv_label_set_text(l, txt);
    lv_obj_center(l);

    btn[id] = b;
    return b;
}

/* Fila tocable: etiqueta a la izquierda, valor exacto a la derecha. Sin botones
 * -/+: la fila entera es el control; tocarla la selecciona y la rueda ajusta la
 * seleccionada. */
static void control_row(lv_obj_t *parent, int32_t y, const char *name, dfl_sel_t s, dfl_ui_id_t id) {
    lv_obj_t *r = lv_obj_create(parent);

    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, DFL_ROW_W, DFL_ROW_H);
    lv_obj_set_pos(r, DFL_ROW_X, y);
    lv_obj_set_style_radius(r, 4, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(r, TC(popup_border), 0);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(r, (void *)(intptr_t) id);
    lv_obj_add_event_cb(r, row_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *name_l = make_label(r, 8, 13, &sony_18, TC(text_dim), 60);

    lv_label_set_text(name_l, name);

    lv_obj_t *value = make_label(r, 64, 9, &sony_22, TC(text), 188);

    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(value, "0 Hz");

    row[s]       = r;
    row_name[s]  = name_l;
    row_value[s] = value;
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    /* Estetica del tema: superficie solida (no la imagen de dialogo, pensada
     * para la caja grande) con los roles de popup. Opaca a proposito: tapa el
     * waterfall que queda debajo. */
    lv_obj_set_style_bg_img_src(dialog.obj, NULL, 0);
    lv_obj_set_style_bg_color(dialog.obj, TC(popup), 0);
    lv_obj_set_style_bg_opa(dialog.obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog.obj, 2, 0);
    lv_obj_set_style_border_color(dialog.obj, TC(popup_border), 0);
    lv_obj_set_style_radius(dialog.obj, 6, 0);
    lv_obj_set_style_pad_all(dialog.obj, 0, 0);
    lv_obj_set_size(dialog.obj, DFL_W, DFL_H);
    lv_obj_set_pos(dialog.obj, DFL_X, DFL_Y);
    lv_obj_clear_flag(dialog.obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = make_label(dialog.obj, 14, 10, &sony_22, TC(text_head), 250);
    lv_label_set_text(title_label, "DFL - Digital Filter");

    mode_label = make_label(dialog.obj, 274, 12, &sony_22, TC(accent), 130);
    lv_label_set_text(mode_label, "USB");

    tx_label = make_label(dialog.obj, 408, 14, &sony_18, TC(tx), 130);
    lv_obj_set_style_text_align(tx_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(tx_label, "");

    make_button(dialog.obj, 556, 4, 100, DFL_BTN, "Default", DFL_UI_RESET);
    make_button(dialog.obj, 664, 4, DFL_BTN, DFL_BTN, "X", DFL_UI_CLOSE);

    graph = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(graph);
    lv_obj_set_size(graph, 432, 128);
    lv_obj_set_pos(graph, 12, 48);
    lv_obj_set_style_bg_color(graph, TC(panel), 0);
    lv_obj_set_style_bg_opa(graph, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(graph, 1, 0);
    lv_obj_set_style_border_color(graph, TC(popup_border), 0);
    lv_obj_set_style_radius(graph, 3, 0);
    lv_obj_add_flag(graph, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(graph, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(graph, (void *)(intptr_t) DFL_UI_GRAPH);
    lv_obj_add_event_cb(graph, draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(graph, graph_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(graph, graph_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(graph, graph_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(graph, graph_event_cb, LV_EVENT_PRESS_LOST, NULL);

    lv_obj_t *hint_label = make_label(dialog.obj, 16, 178, &sony_14, TC(text_dim), 0);
    lv_label_set_text(hint_label, "Esquema de banda pasante");

    control_row(dialog.obj, 54, "LOW", DFL_SEL_LOW, DFL_UI_LOW_ROW);
    control_row(dialog.obj, 98, "HIGH", DFL_SEL_HIGH, DFL_UI_HIGH_ROW);
    control_row(dialog.obj, 142, "BW", DFL_SEL_BW, DFL_UI_BW_ROW);

    /* Estado inicial de TX: el fixture/la radio ya lo sabe al abrir. */
    tx_active = (radio_get_state() != RADIO_RX);

    /* Seleccion inicial al abrir: LOW donde es editable; en AM/SAM/NFM/WFM,
     * donde LOW no es editable, HIGH (ver dfl_default_sel). */
    sel = dfl_default_sel(subject_get_int(cfg_cur.mode));

    lv_obj_add_event_cb(dialog.obj, tx_event_cb, EVENT_RADIO_TX, NULL);
    lv_obj_add_event_cb(dialog.obj, tx_event_cb, EVENT_RADIO_RX, NULL);

    obs_mode = subject_add_delayed_observer(cfg_cur.mode, on_subject_change, NULL);
    obs_low  = subject_add_delayed_observer(cfg_cur.filter.low, on_subject_change, NULL);
    obs_high = subject_add_delayed_observer(cfg_cur.filter.high, on_subject_change, NULL);
    obs_bw   = subject_add_delayed_observer(cfg_cur.filter.bw, on_subject_change, NULL);

    if (keyboard_group) {
        lv_group_add_obj(keyboard_group, dialog.obj);
        lv_group_focus_obj(dialog.obj);
    }

    dfl_refresh();
}

static void destruct_cb(void) {
    dragging = DFL_HANDLE_NONE;

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

    graph      = NULL;
    mode_label = NULL;
    tx_label   = NULL;

    for (int i = 0; i < DFL_SEL_COUNT; i++) {
        row[i]       = NULL;
        row_name[i]  = NULL;
        row_value[i] = NULL;
    }

    for (int i = 0; i <= DFL_UI_CLOSE; i++) {
        btn[i] = NULL;
    }
}

static void key_cb(lv_event_t *e) {
    if (lv_event_get_key(e) == LV_KEY_ESC) {
        dialog_destruct();
    }
}

static dialog_t dialog = {
    .obj          = NULL,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .audio_cb     = NULL,
    .rotary_cb    = rotary_cb,
    .btn_page     = &buttons_page_dfl_editor,
    .prev_page    = NULL,
    .key_cb       = key_cb,
    .restore_radio = false,
    .run          = false,
};

dialog_t *dialog_dfl = &dialog;

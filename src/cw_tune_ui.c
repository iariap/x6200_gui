/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2024 Georgy Dyuldin aka R2RFE
 */

#include "cw_tune_ui.h"

#include <math.h>

#include "styles.h"
#include "params/params.h"
#include "pubsub_ids.h"

#define BLOCK_W 5
#define SPACING 4
#define N_BLOCKS 15
#define WIDTH (N_BLOCKS * (BLOCK_W + SPACING) + SPACING)
#define HEIGHT 40
#define BLOCK_HZ 10

static lv_draw_rect_dsc_t rect_dsc;
static lv_draw_rect_dsc_t rect_active_dsc;

static lv_color_t color_ok;
static lv_color_t color_good;
static lv_color_t color_bad;

static lv_obj_t     *obj;

static int8_t cur_freq=-100;

/* Borde del bloque activo: blanco o negro, el que mas contraste tenga sobre el relleno.
 * Mismo criterio y misma cuenta que band_plan.c (cruce de WCAG 2.1 en L = 0.179). */
#define LUM_CORTE 0.18

static float rel_lum(lv_color_t c)
{
    lv_color32_t c32 = { .full = lv_color_to32(c) };
    float        v[3] = { c32.ch.red / 255.0f, c32.ch.green / 255.0f, c32.ch.blue / 255.0f };

    for (int i = 0; i < 3; i++) {
        v[i] = v[i] <= 0.03928f ? v[i] / 12.92f : powf((v[i] + 0.055f) / 1.055f, 2.4f);
    }
    return 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
}

static void update_cb(lv_event_t * e);
static void update_visibility(Subject *subj, void *user_data);
static void obj_deleted_cb(lv_event_t *e);

void cw_tune_init(lv_obj_t *parent)
{
    color_ok = lv_color_hex(COLOR_LIGHT_GREEN);
    color_good = lv_color_hex(COLOR_LIGHT_YELLOW);
    color_bad = lv_color_hex(COLOR_LIGHT_RED);

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_hex(0x7f7f7f);
    rect_dsc.radius = 5;
    rect_dsc.bg_opa = LV_OPA_50;
    /* Las barras inactivas llevan borde de alto contraste contra el panel del widget (el color se
     * elige en update_cb, porque depende del tema). Ancho 1 y no 2 como el bloque activo: la barra
     * mide BLOCK_W=5 px de ancho y con 2 px de borde el relleno que marca el tono casi desaparece
     * (docs/HALLAZGOS.md 49). */
    rect_dsc.border_width = 1;
    rect_dsc.border_opa = LV_OPA_COVER;

    lv_draw_rect_dsc_init(&rect_active_dsc);
    rect_active_dsc.radius = 5;
    rect_active_dsc.bg_opa = LV_OPA_70;
    /* El bloque activo lleva borde blanco o negro, el que mas contraste tenga sobre su propio
     * relleno (el mismo criterio que band_plan.c): asi el bloque que marca el ajuste se distingue
     * del resto en cualquier tema, que con el relleno solo daba 1.65:1 medido (docs/HALLAZGOS.md 26). */
    rect_active_dsc.border_width = 2;
    rect_active_dsc.border_opa = LV_OPA_COVER;

    obj = lv_obj_create(parent);
    lv_obj_set_height(obj, HEIGHT);
    lv_obj_set_width(obj, WIDTH);
    lv_obj_add_style(obj, &cw_tune_style, 0);

    lv_obj_add_event_cb(obj, update_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(obj, obj_deleted_cb, LV_EVENT_DELETE, NULL);
    subject_add_delayed_observer(cfg_cur.mode, update_visibility, NULL);
    update_visibility(cfg_cur.mode, NULL);
}

// bool cw_tune_toggle(int16_t diff) {
//     if (diff) {
//         params_lock();
//         params.cw_tune = !params.cw_tune;
//         params_unlock(&params.dirty.cw_tune);
//         lv_msg_send(MSG_PARAM_CHANGED, NULL);
//     }
//     // TODO: replace with observer
//     update_visibility(NULL, NULL);
//     return params.cw_tune;
// }

void cw_tune_set_freq(float hz) {
    int8_t new_id = N_BLOCKS / 2 - roundf(hz / BLOCK_HZ);
    if (new_id < 0) new_id = 0;
    if (new_id > N_BLOCKS - 1) new_id = N_BLOCKS - 1;

    if (LV_ABS(hz) <= 10) {
        rect_active_dsc.bg_color = color_ok;
    } else if (LV_ABS(hz) <= 20) {
        rect_active_dsc.bg_color = color_good;
    } else {
        rect_active_dsc.bg_color = color_bad;
    }
    rect_active_dsc.border_color = rel_lum(rect_active_dsc.bg_color) < LUM_CORTE ? lv_color_white()
                                                                                 : lv_color_black();
    if (cur_freq != new_id){
        cur_freq = new_id;
        lv_event_send(obj, LV_EVENT_REFRESH, NULL);
    }
}

/* Superficie sobre la que se apoyan las barras: el panel del widget es negro al 50% sobre el fondo
 * del tema (`lv_style_set_opa(&cw_tune_style, LV_OPA_50)`, src/styles.c), asi que su color es el
 * del fondo a la mitad de cada canal. */
static lv_color_t panel_color(void)
{
    lv_color32_t c32 = { .full = lv_color_to32(lv_obj_get_style_bg_color(lv_obj_get_parent(obj), LV_PART_MAIN)) };

    return lv_color_make(c32.ch.red / 2, c32.ch.green / 2, c32.ch.blue / 2);
}

static void update_cb(lv_event_t * e) {
    int16_t x, h;
    int16_t y_b=HEIGHT - 1;
    int16_t w=BLOCK_W;

    lv_draw_ctx_t * ctx = lv_event_get_draw_ctx(e);
    lv_draw_rect_dsc_t * dsc;
    lv_area_t coords, offset;

    lv_obj_get_coords(obj, &offset);

    /* El borde de las barras inactivas se elige contra el PANEL que las rodea y no contra su propio
     * relleno: el relleno es un gris fijo, y sobre el panel del tema claro cae en el mismo valor
     * (1.01:1 medido, docs/HALLAZGOS.md 49). Mismo criterio y misma cuenta que band_plan.c y que el
     * bloque activo: blanco o negro, el que mas contraste tenga contra esa superficie (>= 3:1 en los
     * diez temas, el minimo de WCAG 1.4.11 para un componente). Se recalcula en cada dibujo porque
     * depende del tema. */
    rect_dsc.border_color = rel_lum(panel_color()) < LUM_CORTE ? lv_color_white() : lv_color_black();

    for (int16_t i = 0; i < N_BLOCKS; i++) {
        x = SPACING + i * (BLOCK_W + SPACING);
        h = (HEIGHT - 3) * 2 / (LV_ABS(i - N_BLOCKS / 2) + 2);
        if (cur_freq == i) {
            dsc = &rect_active_dsc;
        } else {
            dsc = &rect_dsc;
        }
        coords.x1 = x;
        coords.y1 = y_b - h;
        coords.x2 = coords.x1 + w;
        coords.y2 = coords.y1 + h;
        lv_area_move(&coords, offset.x1, offset.y1);
        lv_draw_rect(ctx, dsc, &coords);
    }
}

/* Cuando el objeto se destruye (por ejemplo al reconstruirse la pantalla principal al abrir una
 * app), el observador del modo sigue vivo: sin esto quedaba un puntero colgado y el primer cambio
 * de modo despues tocaba memoria liberada y mataba la GUI. Medido con el core: el SIGSEGV era en
 * lv_obj_add_flag -> lv_obj_has_flag_any (docs/HALLAZGOS.md 34). */
static void obj_deleted_cb(lv_event_t *e) {
    /* Solo si el que muere es el objeto que apuntamos: cuando la pantalla se reconstruye, el viejo
     * se destruye despues de crear el nuevo y no hay que borrar el puntero nuevo. */
    if (obj == (lv_obj_t *) lv_event_get_target(e)) {
        obj = NULL;
    }
}

static void update_visibility(Subject *subj, void *user_data) {
    if (obj == NULL) {
        return;
    }

    /* El widget es la herramienta de sintonia de CW: se muestra en CW/CWR y en ningun otro modo.
     * Antes ademas exigia el parametro `cw_tune` (el boton "Tuner" de la pagina CW), que arranca en
     * 0 (src/cfg/cfg.c:349) y dejaba el widget invisible en CW: era la causa del issue #122. El
     * parametro y su boton siguen existiendo, pero ya no deciden la visibilidad. */
    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    bool on = (mode == x6200_mode_cw) || (mode == x6200_mode_cwr);
    if (on) {
        if (obj) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (obj) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }

}

/* See the note in cw_tune_ui.h */
bool cw_tune_ui_visible(void) {
    return (obj != NULL) && (!lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN));
}

/* See the note in cw_tune_ui.h */
bool cw_tune_recent(int32_t *hz) {
    (void) hz;

    return false;
}

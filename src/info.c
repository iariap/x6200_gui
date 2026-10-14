/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "info.h"

#include "btaudio.h"
#include "theme.h"
#include "bt_state.h"
#include "cfg/transverter.h"
#include "cfg/mode.h"
#include "fonts/font_select.h"
#include "styles.h"
#include "params/params.h"
#include "pubsub_ids.h"
#include "ui_tokens.h"      /* la geometria de la barra (issue #10, UI-05) */
#include "wifi.h"

#include <ctype.h>

typedef enum {
    INFO_VFO = 0,
    INFO_MODE,
    INFO_AGC,
    INFO_PRE_ATT,
    INFO_ATU,
    INFO_WIFI
} info_items_t;

static lv_obj_t     *obj;
static lv_obj_t     *items[UI_INFO_COLS * UI_INFO_ROWS];

static Subject *mode_lock;

/* Icono de Bluetooth de la barra (ver info_init). Se refresca con un timer de 2 s: btcat-ctl corre
 * en un hilo y escribe la configuracion un rato despues, y ademas hay cambios que hace btaudio-ctl
 * por fuera de la GUI. Cuesta dos archivos chicos y un access(), y es el mismo patron que ya usa
 * dialog_bt.c (700 ms) porque LVGL no es thread-safe: no se puede avisar desde el hilo. */
static lv_obj_t *bt_item;

/* Runa del Bluetooth, en 14x20: un solo trazo de 7 puntos (el glifo LV_SYMBOL_BLUETOOTH no esta en
 * las fuentes del equipo: sony_20 solo trae wifi y bateria, y salia un rectangulo vacio). */
static lv_point_t bt_pts[] = {
    {  7,  0 },
    { 14,  5 },
    {  0, 15 },
    {  7, 20 },
    { 14, 15 },
    {  0,  5 },
    {  7,  0 },
};

static void bt_state_show(void);
static void bt_state_timer_cb(lv_timer_t *t);

static void wifi_state_change_cb(void *s, lv_msg_t *m);

static void vfo_label_update(Subject *subj, void * user_data);
static void mode_label_update(Subject *subj, void * user_data);
static void atu_label_update(Subject *subj, void * user_data);
static void agc_label_update(Subject *subj, void * user_data);
static void att_pre_label_update(Subject *subj, void * user_data);

lv_obj_t * info_init(lv_obj_t * parent) {

    mode_lock = subject_create_int(false);

    obj = lv_obj_create(parent);

    lv_obj_add_style(obj, &info_style, 0);
    if (obj) lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    uint8_t index = 0;

    for (uint8_t y = 0; y < UI_INFO_ROWS; y++)
        for (uint8_t x = 0; x < UI_INFO_COLS; x++) {
            lv_obj_t *item = lv_label_create(obj);

            lv_obj_add_style(item, &info_item_style, 0);
            lv_obj_set_pos(item, x * UI_INFO_COL_PITCH + UI_INFO_ORIGIN_X,
                           y * UI_INFO_ROW_PITCH + UI_INFO_ORIGIN_Y);
            lv_obj_set_style_text_align(item, LV_TEXT_ALIGN_CENTER, 0);

            /* El fondo de la barra es un bitmap (images/top_short*.bin) con zonas claras y oscuras,
             * asi que un texto fijo no puede cumplir WCAG contra el: medido en una captura, el gris
             * #99A0A7 y el rojo #FF8080 daban 2.3 y 2.0:1 sobre las partes blancas. Cada item pinta
             * su celda con TC(bar) y el texto con los roles del tema, que el chequeo valida en los 9
             * temas (docs/HALLAZGOS.md 28). */
            lv_obj_set_style_bg_color(item, TC(bar), 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(item, UI_INFO_CELL_RADIUS, 0);
            lv_obj_set_style_text_color(item, TC(text), 0);
            lv_obj_set_size(item, UI_INFO_CELL_W, UI_INFO_CELL_H);

            items[index] = item;
            index++;
        }

    /* La celda del VFO es la unica que no entra cuando VISION escala las fuentes
     * (fonts_scale, tres pasos): las seis celdas siguen siendo las 56x26 fijas del
     * bitmap de la barra (arriba) y `VFO-A` pasa de 48 a 66 px de ancho, mas que
     * la celda, asi que el WRAP la parte en dos lineas y la celda recorta la
     * segunda: en pantalla queda "VFO-" y el "A" no se ve (UI-012). Las otras
     * cinco celdas ya entran con la fuente grande, asi que solo el VFO vuelve al
     * tamano de los otros temas -- el mismo idioma que NF() en
     * src/cluster_column.c, que tambien tiene columnas de ancho fijo. */
    lv_obj_set_style_text_font(items[INFO_VFO],
                               theme_shape.vision ? font_down(UI_INFO_FONT, VISION_FONT_STEPS) : UI_INFO_FONT,
                               0);

    lv_label_set_text(items[INFO_PRE_ATT], "PRE/ATT");
    lv_label_set_text(items[INFO_WIFI], LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(items[INFO_WIFI], TC(text_dim), 0);

    /* Icono de Bluetooth, al lado del de WiFi: la celda es de 56 px, asi que el WiFi se corre a la
     * mitad izquierda y el BT ocupa la derecha (el S-meter empieza justo al lado de la barra, no se
     * puede agregar una columna). Tres estados, como el WiFi: apagado (negro), prendido sin
     * auriculares (gris) y con auriculares A2DP conectados (blanco). Se refresca cada 2 s porque
     * btcat-ctl escribe el estado en un hilo, con un retraso (docs/HALLAZGOS.md 19). */
    lv_obj_set_size(items[INFO_WIFI], UI_INFO_WIFI_W, UI_INFO_CELL_H);
    lv_obj_set_style_text_align(items[INFO_WIFI], LV_TEXT_ALIGN_LEFT, 0);

    bt_item = lv_line_create(obj);
    lv_line_set_points(bt_item, bt_pts, UI_INFO_BT_POINTS);
    lv_obj_set_style_line_width(bt_item, UI_INFO_BT_LINE_W, 0);
    lv_obj_set_style_line_rounded(bt_item, true, 0);
    lv_obj_set_pos(bt_item, UI_INFO_WIFI_COL * UI_INFO_COL_PITCH + UI_INFO_ORIGIN_X + UI_INFO_WIFI_W,
                   UI_INFO_WIFI_ROW * UI_INFO_ROW_PITCH + UI_INFO_BT_DY);

    bt_state_show();
    lv_timer_create(bt_state_timer_cb, 2000, NULL);

    subject_add_delayed_observer(cfg_cur.band->vfo.val, vfo_label_update, NULL);
    subject_add_delayed_observer(cfg_cur.band->split.val, vfo_label_update, NULL);
    vfo_label_update(cfg_cur.band->vfo.val, NULL);

    subject_add_delayed_observer(cfg_cur.mode, mode_label_update, NULL);
    subject_add_delayed_observer(mode_lock, mode_label_update, NULL);
    mode_label_update(cfg_cur.band->vfo.val, NULL);

    subject_add_delayed_observer(cfg.ant_id.val, atu_label_update, NULL);
    subject_add_delayed_observer(cfg_cur.fg_freq, atu_label_update, NULL);
    subject_add_delayed_observer(cfg_cur.atu->loaded, atu_label_update, NULL);
    subject_add_delayed_observer(cfg.atu_enabled.val, atu_label_update, NULL);
    atu_label_update(cfg.atu_enabled.val, NULL);

    subject_add_delayed_observer(cfg_cur.agc, agc_label_update, NULL);
    agc_label_update(cfg_cur.agc, NULL);

    subject_add_delayed_observer(cfg_cur.att, att_pre_label_update, NULL);
    subject_add_delayed_observer(cfg_cur.pre, att_pre_label_update, NULL);
    att_pre_label_update(cfg_cur.pre, NULL);

    lv_msg_subscribe(MSG_WIFI_STATE_CHANGED, wifi_state_change_cb, NULL);

    return obj;
}

const char* info_params_mode_label_get() {
    x6200_mode_t    mode = subject_get_int(cfg_cur.mode);
    char            *str;

    switch (mode) {
        case x6200_mode_lsb:
            str = "LSB";
            break;

        case x6200_mode_lsb_dig:
            str = "LSB-D";
            break;

        case x6200_mode_usb:
            str = "USB";
            break;

        case x6200_mode_usb_dig:
            str = "USB-D";
            break;

        case x6200_mode_cw:
            str = "CW";
            break;

        case x6200_mode_cwr:
            str = "CW-R";
            break;

        case x6200_mode_am:
            str = "AM";
            break;

        case x6200_mode_sam:
            str = "SAM";
            break;

        case x6200_mode_nfm:
            str = "NFM";
            break;

        case x6200_mode_wfm:
            str = "WFM";
            break;

        default:
            str = "?";
            break;
    }

    return str;
}

const char* info_params_vfo_label_get() {
    x6200_vfo_t cur_vfo = subject_get_int(cfg_cur.band->vfo.val);
    char            *str;

    if (subject_get_int(cfg_cur.band->split.val)) {
        str = cur_vfo == X6200_VFO_A ? "SPL-A" : "SPL-B";
    } else {
        str = cur_vfo == X6200_VFO_A ? "VFO-A" : "VFO-B";
    }

    return str;
}

void info_lock_mode(bool lock) {
    subject_set_int(mode_lock, lock);
}

static void wifi_state_change_cb(void *s, lv_msg_t *m) {
    lv_color_t color;
    switch (wifi_get_status()) {
    case WIFI_CONNECTED:
        color = TC(text);
        break;
    case WIFI_OFF:
        color = TC(icon_off);
        break;
    default:
        color = TC(text_dim);
        break;
    }
    lv_obj_set_style_text_color(items[INFO_WIFI], color, 0);
}

/* Icono de Bluetooth: mismo criterio de colores que el WiFi (blanco = listo, gris = a medias,
 * negro = apagado). Con auriculares A2DP conectados queda blanco. */
static void bt_state_show(void) {
    lv_color_t color;

    if (!bt_state_on()) {
        color = TC(icon_off);         /* apagado: exento (1.4.3) */
    } else if (bt_state_connected()) {
        color = TC(text);
    } else {
        color = TC(text_dim);
    }

    if (bt_item != NULL) {
        lv_obj_set_style_line_color(bt_item, color, 0);
    }
}

static void bt_state_timer_cb(lv_timer_t *t) {
    (void) t;

    bt_state_show();
    btaudio_follow_state();     /* el mute del altavoz con auriculares (BTAUDIO_MUTE) */
}


static void vfo_label_update(Subject *subj, void * user_data) {
    lv_label_set_text(items[INFO_VFO], info_params_vfo_label_get());
}

static void mode_label_update(Subject *subj, void *user_data) {
    lv_label_set_text(items[INFO_MODE], info_params_mode_label_get());
    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    if ((mode == x6200_mode_lsb_dig) || (mode == x6200_mode_usb_dig)) {
        lv_obj_set_style_text_color(items[INFO_MODE], TC(mode_dig), 0);
    } else if (subject_get_int(mode_lock)) {
        lv_obj_set_style_text_color(items[INFO_MODE], TC(text_mute), 0);
    } else {
        lv_obj_set_style_text_color(items[INFO_MODE], TC(text), 0);
    }
}

static void atu_label_update(Subject *subj, void * user_data) {
    int32_t ant = subject_get_int(cfg.ant_id.val);
    lv_label_set_text_fmt(items[INFO_ATU], "ATU%i", ant);
    int32_t freq = subject_get_int(cfg_cur.fg_freq);

    if (!subject_get_int(cfg.atu_enabled.val)) {
        lv_obj_set_style_text_color(items[INFO_ATU], TC(text), 0);
        lv_obj_set_style_bg_color(items[INFO_ATU], TC(bar), 0);
        lv_obj_set_style_bg_opa(items[INFO_ATU], LV_OPA_COVER, 0);
    } else {
        if (cfg_transverter_get_shift(freq)) {
            lv_obj_set_style_text_color(items[INFO_ATU], TC(text_dim), 0);
            lv_obj_set_style_bg_color(items[INFO_ATU], TC(bar), 0);
            lv_obj_set_style_bg_opa(items[INFO_ATU], LV_OPA_COVER, 0);
        } else {
            /* Cargado o no: roles del tema. El rojo fijo #FF0000 no servia (daba ~2:1 sobre las partes
             * claras del bitmap) y ningun rol "rojo" del tema llega a 4.5 sobre TC(bar) -tx 3.06 en
             * IU1WXY, 1.97 en Legacy, medido-, asi que el ATU sin cargar queda atenuado (text_dim),
             * que es lo que el chequeo valida (docs/HALLAZGOS.md 30). */
            bool cargado = subject_get_int(cfg_cur.atu->loaded);

            lv_obj_set_style_bg_color(items[INFO_ATU], cargado ? TC(sel_bg) : TC(bar), 0);
            lv_obj_set_style_bg_opa(items[INFO_ATU], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(items[INFO_ATU], cargado ? TC(sel_text) : TC(text_dim), 0);
        }
    }
}

static void agc_label_update(Subject *subj, void *user_data) {
    char label[8];
    strcpy(label, cfg_mode_agc_label(subject_get_int(cfg_cur.agc)));
    for (size_t i = 0; i < strlen(label); i++) {
        label[i] = toupper(label[i]);
    }
    lv_label_set_text(items[INFO_AGC], label);
}

static void att_pre_label_update(Subject *subj, void * user_data) {
    /* Activo: el par "seleccionado" del tema (bg/texto, validado >=4.5 en los 9 temas); inactivo:
     * el fondo de la celda y el texto normal. Antes era negro sobre blanco al 50%, que sobre el
     * fondo del tema daba texto claro sobre celda clara (medido en el banco, docs/HALLAZGOS.md 30). */
    bool activo = subject_get_int(cfg_cur.att) || subject_get_int(cfg_cur.pre);

    if (subject_get_int(cfg_cur.att)) {
        lv_label_set_text(items[INFO_PRE_ATT], "ATT");
    } else if (subject_get_int(cfg_cur.pre)) {
        lv_label_set_text(items[INFO_PRE_ATT], "PRE");
    } else {
        lv_label_set_text(items[INFO_PRE_ATT], "P/A");
    }

    lv_obj_set_style_bg_color(items[INFO_PRE_ATT], activo ? TC(sel_bg) : TC(bar), 0);
    lv_obj_set_style_bg_opa(items[INFO_PRE_ATT], LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(items[INFO_PRE_ATT], activo ? TC(sel_text) : TC(text), 0);
}

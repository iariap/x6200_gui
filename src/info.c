/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "info.h"

#include "btaudio.h"
#include "cb.h"
#include "theme.h"
#include "bt_state.h"
#include "cfg/transverter.h"
#include "cfg/mode.h"
#include "fonts/font_select.h"
#include "styles.h"
#include "params/params.h"
#include "pubsub_ids.h"
#include "ui_tokens.h"      /* la geometria de la barra (issue #10, UI-05) */
#include "ui_set.h"         /* guardas de igualdad (OPT-4 #150) */
#include "wifi.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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

/* Fuente de los tres estados compactos de la barra (AGC, PRE/ATT, ATU): el slot
 * grande de la celda (UI_INFO_FONT_BIG, #251; su nota en src/ui_tokens.h deja
 * medido que en las familias de la coleccion ese slot no agranda el glifo). Bajo
 * VISION baja los mismos tres pasos que la celda del VFO -- `fonts_scale()` sube
 * todas las fuentes en VISION_FONT_STEPS -- asi que el tamano efectivo no cambia
 * al escalar y el texto no envuelve ni recorta en la celda fija de 56x26. Mismo
 * idioma que `NF()` en src/cluster_column.c. */
static const lv_font_t *status_font_big(void) {
    return theme_shape.vision ? font_down(UI_INFO_FONT_BIG, VISION_FONT_STEPS) : UI_INFO_FONT_BIG;
}

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

    for (uint8_t y = 0; y < UI_INFO_ROWS; y++) {
        /* Una fila = un contenedor flex (LV_FLEX_ROW): el flex gobierna el eje X
         * de las tres celdas y el gap sale de UI_INFO_CELL_GAP, en vez de sumar
         * UI_INFO_COL_PITCH a mano por celda. El contenedor queda en el origen
         * exacto de la fila (UI_INFO_ROW_PITCH), asi que las filas ya no se
         * solapan: el pitch (26) es el alto de la celda (#251). Las celdas
         * mantienen su tamano fijo (56x26): flex coloca, no redimensiona
         * (ninguna lleva flex_grow). */
        lv_obj_t *row = lv_obj_create(obj);

        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_pad_column(row, UI_INFO_CELL_GAP, 0);
        lv_obj_set_size(row,
                        UI_INFO_COLS * UI_INFO_CELL_W + (UI_INFO_COLS - 1) * UI_INFO_CELL_GAP,
                        UI_INFO_CELL_H);
        lv_obj_set_pos(row, UI_INFO_ORIGIN_X, y * UI_INFO_ROW_PITCH + UI_INFO_ORIGIN_Y);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        for (uint8_t x = 0; x < UI_INFO_COLS; x++) {
            lv_obj_t *item = lv_label_create(row);

            lv_obj_add_style(item, &info_item_style, 0);
            lv_obj_set_style_text_align(item, LV_TEXT_ALIGN_CENTER, 0);
            /* Padding vertical LOCAL de la celda (#251): info_item_style trae 5 px
             * por lado y el texto de estado de 22 px necesita las 22 px utiles de
             * la celda (26 - 2*2). Se fija sobre el objeto para no tocar el estilo
             * que comparten las seis celdas. */
            lv_obj_set_style_pad_top(item, UI_INFO_CELL_PAD_V, 0);
            lv_obj_set_style_pad_bottom(item, UI_INFO_CELL_PAD_V, 0);

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
    }

    /* Dos de las seis celdas no entran cuando VISION escala las fuentes
     * (fonts_scale, tres pasos): las seis siguen siendo las 56x26 fijas del bitmap
     * de la barra (arriba) y con la fuente grande `VFO-A` pasa de 48 a 66 px de
     * ancho, mas que la celda, asi que el WRAP la parte en dos lineas y la celda
     * recorta la segunda: en pantalla queda "VFO-" y el "A" no se ve (UI-012). La
     * celda del modo tiene el mismo problema y se mide segun el modo: `CW-R` mide
     * exactamente 56 px, el ancho de la celda, asi que queda con 0 px de margen y
     * se pega a la celda de AGC (que muestra "AUTO" -> en pantalla se lee
     * "CW-RAUTO", UI-014), y `USB-D` mide 66 px y tambien se parte y se recorta.
     * Las dos celdas vuelven al tamano de los otros temas -- el mismo idioma que
     * NF() en src/cluster_column.c, que tambien tiene columnas de ancho fijo. */
    lv_obj_set_style_text_font(items[INFO_VFO],
                               theme_shape.vision ? font_down(UI_INFO_FONT, VISION_FONT_STEPS) : UI_INFO_FONT,
                               0);
    lv_obj_set_style_text_font(items[INFO_MODE],
                               theme_shape.vision ? font_down(UI_INFO_FONT, VISION_FONT_STEPS) : UI_INFO_FONT,
                               0);

    /* Los otros tres textos son cortos -- medidos en el arnes: `AUTO`/`FAST` 43/39
     * px, `ATU1` 37, `P/A` 25 -- y son los estados que el operador lee de un
     * vistazo, asi que dibujan con el slot grande de la celda (sony_22, #251) sin
     * acercarse al borde de los 56 px. La fuente va sobre el objeto (no en
     * info_item_style, que comparten las seis celdas) y es `status_font_big()`, que
     * ademas los mantiene en ese tamano efectivo bajo VISION. La celda de WiFi/BT
     * NO la usa: el WiFi y la runa de BT conservan su representacion nativa, y el
     * padding de la celda (UI_INFO_CELL_PAD_V, arriba) les deja el texto de 18 px
     * de alto en las 22 px utiles. */
    lv_obj_set_style_text_font(items[INFO_AGC], status_font_big(), 0);
    lv_obj_set_style_text_font(items[INFO_PRE_ATT], status_font_big(), 0);
    lv_obj_set_style_text_font(items[INFO_ATU], status_font_big(), 0);

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
    /* La celda tambien depende de la frecuencia (en CB pasa a "CH nn") y de la
     * banda (al cambiar de banda la frecuencia se recarga); issue #146. */
    subject_add_delayed_observer(cfg_cur.fg_freq, vfo_label_update, NULL);
    subject_add_delayed_observer(cfg.band_id.val, vfo_label_update, NULL);
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
    ui_obj_set_text_color(items[INFO_WIFI], color);
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

    /* Guarda de igualdad (OPT-4 #150): el timer de 2 s reescribe el color del
     * icono aunque el estado de BT no haya cambiado; set_style invalida siempre.
     * Se compara contra el color resuelto del objeto (sin cache). */
    if (bt_item != NULL && lv_obj_get_style_line_color(bt_item, 0).full != color.full) {
        lv_obj_set_style_line_color(bt_item, color, 0);
    }
}

/* Guarda de igualdad para el color de fondo + opa de una celda (mismo criterio
 * que ui_obj_set_text_color, para los pares que cambian juntos). */
static void info_set_bg(lv_obj_t *obj, lv_color_t color, lv_opa_t opa) {
    if (lv_obj_get_style_bg_color(obj, 0).full != color.full) {
        lv_obj_set_style_bg_color(obj, color, 0);
    }
    if (lv_obj_get_style_bg_opa(obj, 0) != opa) {
        lv_obj_set_style_bg_opa(obj, opa, 0);
    }
}

static void bt_state_timer_cb(lv_timer_t *t) {
    (void) t;

    bt_state_show();
    btaudio_follow_state();     /* el mute del altavoz con auriculares (BTAUDIO_MUTE) */
}


static void vfo_label_update(Subject *subj, void * user_data) {
    /* En CB la celda VFO pasa a contar canales (issue #146, seccion 6.2): "CH nn"
     * si la frecuencia es un canal exacto del plan, "CH --" si cae en un hueco o
     * entre canales. Fuera de CB conserva el rotulo de siempre (VFO-A / SPL-B). */
    char label[8];

    if (cb_channel_label(subject_get_int(cfg_cur.fg_freq), label, sizeof(label))) {
        ui_label_set_text(items[INFO_VFO], label);
    } else {
        ui_label_set_text(items[INFO_VFO], info_params_vfo_label_get());
    }
}

static void mode_label_update(Subject *subj, void *user_data) {
    ui_label_set_text(items[INFO_MODE], info_params_mode_label_get());
    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    if ((mode == x6200_mode_lsb_dig) || (mode == x6200_mode_usb_dig)) {
        ui_obj_set_text_color(items[INFO_MODE], TC(mode_dig));
    } else if (subject_get_int(mode_lock)) {
        ui_obj_set_text_color(items[INFO_MODE], TC(text_mute));
    } else {
        ui_obj_set_text_color(items[INFO_MODE], TC(text));
    }
}

static void atu_label_update(Subject *subj, void * user_data) {
    int32_t ant = subject_get_int(cfg.ant_id.val);
    char    label[8];

    snprintf(label, sizeof(label), "ATU%i", ant);
    ui_label_set_text(items[INFO_ATU], label);

    int32_t freq = subject_get_int(cfg_cur.fg_freq);

    if (!subject_get_int(cfg.atu_enabled.val)) {
        ui_obj_set_text_color(items[INFO_ATU], TC(text));
        info_set_bg(items[INFO_ATU], TC(bar), LV_OPA_COVER);
    } else {
        if (cfg_transverter_get_shift(freq)) {
            ui_obj_set_text_color(items[INFO_ATU], TC(text_dim));
            info_set_bg(items[INFO_ATU], TC(bar), LV_OPA_COVER);
        } else {
            /* Cargado o no: roles del tema. El rojo fijo #FF0000 no servia (daba ~2:1 sobre las partes
             * claras del bitmap) y ningun rol "rojo" del tema llega a 4.5 sobre TC(bar) -tx 3.06 en
             * IU1WXY, 1.97 en Legacy, medido-, asi que el ATU sin cargar queda atenuado (text_dim),
             * que es lo que el chequeo valida (docs/HALLAZGOS.md 30). */
            bool cargado = subject_get_int(cfg_cur.atu->loaded);

            info_set_bg(items[INFO_ATU], cargado ? TC(sel_bg) : TC(bar), LV_OPA_COVER);
            ui_obj_set_text_color(items[INFO_ATU], cargado ? TC(sel_text) : TC(text_dim));
        }
    }
}

static void agc_label_update(Subject *subj, void *user_data) {
    char label[8];
    strcpy(label, cfg_mode_agc_label(subject_get_int(cfg_cur.agc)));
    for (size_t i = 0; i < strlen(label); i++) {
        label[i] = toupper(label[i]);
    }
    ui_label_set_text(items[INFO_AGC], label);
}

static void att_pre_label_update(Subject *subj, void * user_data) {
    /* Activo: el par "seleccionado" del tema (bg/texto, validado >=4.5 en los 9 temas); inactivo:
     * el fondo de la celda y el texto normal. Antes era negro sobre blanco al 50%, que sobre el
     * fondo del tema daba texto claro sobre celda clara (medido en el banco, docs/HALLAZGOS.md 30). */
    bool activo = subject_get_int(cfg_cur.att) || subject_get_int(cfg_cur.pre);

    if (subject_get_int(cfg_cur.att)) {
        ui_label_set_text(items[INFO_PRE_ATT], "ATT");
    } else if (subject_get_int(cfg_cur.pre)) {
        ui_label_set_text(items[INFO_PRE_ATT], "PRE");
    } else {
        ui_label_set_text(items[INFO_PRE_ATT], "P/A");
    }

    info_set_bg(items[INFO_PRE_ATT], activo ? TC(sel_bg) : TC(bar), LV_OPA_COVER);
    ui_obj_set_text_color(items[INFO_PRE_ATT], activo ? TC(sel_text) : TC(text));
}

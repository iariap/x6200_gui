/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "dialog_settings.h"

#include "cfg/transverter.h"
#include "lvgl/lvgl.h"
#include "dialog.h"
#include "main_screen.h"
#include <stdio.h>

#include "styles.h"
#include "theme.h"
#include "fonts/font_select.h"
#include "msg.h"
#include "params/params.h"
#include "backlight.h"
#include "radio.h"
#include "events.h"
#include "keyboard.h"
#include "bt_state.h"
#include "clock.h"
#include "voice.h"
#include "audio.h"
#include "audio_enhancement.h"
#include "waterfall.h"
#include "band_plan.h"
#include "cluster_column.h"
#include "wfview_server.h"
#include "main.h"
#include "visual_policy.h"

#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <errno.h>

static lv_obj_t     *grid;

/* La grilla se comporta como una tabla de HTML al 100%: ocupa el ancho del dialog y la primera
 * columna (las etiquetas) se queda con lo que sobra, asi que nunca desborda. Antes eran tamanos
 * fijos de 57/5 con la grilla de 780 px de ancho -la pantalla del X6100, apaisada-, y en el X6200
 * no entraba en el ancho del dialog: scrolleaba en horizontal y las etiquetas quedaban cortadas
 * ("e (CAT)" en lugar de "Discoverable (CAT)"), justo en las filas del Bluetooth
 * (docs/HALLAZGOS.md 18). El ancho por columna (45) sale de medir la captura: el spinbox del ano
 * con 4 digitos ("2026") necesita 2 columnas de ese ancho para no cortarse. */
#define SMALL_PAD   3

#define SMALL_1     45
#define SMALL_2     (SMALL_1 * 2 + SMALL_PAD * 1)
#define SMALL_3     (SMALL_1 * 3 + SMALL_PAD * 2)
#define SMALL_4     (SMALL_1 * 4 + SMALL_PAD * 3)
#define SMALL_5     (SMALL_1 * 5 + SMALL_PAD * 4)
#define SMALL_6     (SMALL_1 * 6 + SMALL_PAD * 5)

/* LV_GRID_FR(1) = la que se lleva el espacio libre (las etiquetas de la columna 0). */
static lv_coord_t   col_dsc[] = { LV_GRID_FR(1), SMALL_1, SMALL_1, SMALL_1, SMALL_1, SMALL_1, SMALL_1, LV_GRID_TEMPLATE_LAST };

/* --- Esquema unico de la zona de controles (issue #175) ---------------------------------------
 * La columna 0 (FR) es la de las etiquetas; las columnas 1..6 son la ZONA DE CONTROLES (6
 * columnas de 45 + 5 gaps de SMALL_PAD = 285 px utiles, de x=499 a x=784 en 800x480).
 *
 * Cada fila elige a mano su columna inicial y su span, y eso habia dejado TRES origenes para la
 * misma "columna derecha": los controles de una sola pieza arrancaban en la columna 1 (x=499) o en
 * la 4 (x=643), y la fila de 3 campos de Clock en 1/3/5 (499/595/691). Ahora el esquema es UNO:
 *
 *   - TODA fila arranca su bloque de controles en la columna 1 (COL_CONTROLES, x=499). Es el unico
 *     origen de la columna derecha.
 *   - Los spans son fijos por cantidad de controles y tapan la zona entera:
 *       1 control   -> span 6                  (285 px)  499..784
 *       2 controles -> span 3 + span 3         (141 c/u) 499..640 / 643..784
 *       3 controles -> span 2 + span 2 + span 2 (93 c/u) 499..592 / 595..688 / 691..784
 *
 * Los numeros salen de los anchos REALES que declara cada widget (medidos con `ui_harness
 * --inspect`): el desplegable mas ancho (el del tema, con los nombres de los 6 temas) y el
 * desplegable de texto libre son SMALL_6=285 (el ancho util de la zona); el spinbox de 4 cifras
 * ("2026") necesita SMALL_2=93 (2 columnas de 45 + el gap) y por eso las filas de 3 campos usan
 * span 2; los switches y los sliders de ganancia viven en cajas de SMALL_3=141, que es el medio de
 * la zona (3+3). El campo "Anio" de `make_date` (issue #110) es el caso que fija SMALL_1=45: 2
 * columnas de ese ancho son la caja minima de 4 cifras.
 *
 * UNICA EXCEPCION (documentada, no tres): la fila "Timeout, Brightness" de `make_backlight` es
 * 2+4 (span 2 para el spinbox de 3 cifras y span 4 para el slider de brillo). El slider de brillo
 * es un slider de RANGO (idle..normal) y mide SMALL_4-30=159 px: no entra en la mitad de la zona
 * (141) sin recortarse, asi que su caja usa la mitad ancha. Su borde izquierdo cae en la columna 3
 * (x=595), que es una columna real del mismo esquema (la del campo del medio de las filas de 3). */

#define COL_CONTROLES   1   /* el unico origen de la columna derecha (x=499) */
#define SPAN_1          6   /* una pieza: toda la zona (285 px)               */
#define SPAN_2A         3   /* dos piezas: la primera ocupa media zona        */
#define COL_2B          4   /* ... y la segunda arranca en la otra media      */
#define SPAN_2B         3
#define SPAN_3          2   /* tres piezas: un tercio cada una (93 px)        */
                            /* (las filas de 3 arrancan en la col 1 y avanzan de a 2: 1/3/5) */

/* La excepcion de "Timeout, Brightness" (ver arriba): spinbox en la col 1 (span 2) y el slider de
 * rango en la col 3 (span 4). */
#define COL_BRILLO      3
#define SPAN_BRILLO     4
static lv_coord_t   row_dsc[64] = { 1 };

static time_t       now;
struct tm           ts;

static lv_obj_t     *day;
static lv_obj_t     *month;
static lv_obj_t     *year;
static lv_obj_t     *hour;
static lv_obj_t     *min;
static lv_obj_t     *sec;

/* Anuncia un ajuste: el nombre es la etiqueta de la celda (el primer hijo) y el valor lo pasa
 * quien llama. El diccionario de voice_text.c traduce el nombre al espanol. */
static void anunciar_ajuste(lv_obj_t *widget, const char *valor);

static void construct_cb(lv_obj_t *parent);
static void key_cb(lv_event_t * e);

/* --- Los tabs de Ajustes ----------------------------------------------------------------------
 * La fila de botones de abajo estaba vacia: los 25 ajustes se recorrian con la perilla. Ahora
 * estan agrupados por tema y cada grupo tiene su boton, que salta a la seccion (la vista y el foco
 * viajan juntos). Son 8, asi que van en dos paginas de 4 mas el boton de pagina: la fila admite 5
 * (docs/HALLAZGOS.md 31).
 *
 * El ORDEN es por utilidad para hacer contactos (issue #109): primero lo que se toca mientras se
 * opera (Operation, Audio, Spectrum, Clock), despues lo ergonomico (Display, Appearance, Actions) y
 * al final lo eventual (Info). El enum, `tabs_items`, las dos paginas y el armado de la grilla
 * (construct_cb) van todos en ese orden, que es el que se ve en la barra. Los labels van en ingles
 * de ham radio (issue #109); el porque de cada uno, en docs/settings-order.md. */
enum {
    GRP_OPERACION, GRP_AUDIO, GRP_ESPECTRO, GRP_RELOJ,
    GRP_PANTALLA, GRP_APARIENCIA, GRP_ACCIONES, GRP_INFO, GRP_COUNT
};

static uint16_t     grp_primero[GRP_COUNT];   /* primer hijo del grupo dentro del grid */
static uint16_t     grp_ultimo[GRP_COUNT];
static uint8_t      grp_actual = GRP_OPERACION;

static void settings_goto(uint8_t g);
static void tabs_marks(void);

/* Cada tab salta a su grupo. button_item_t.press recibe el item, asi que cada uno necesita su
 * propia funcion (una macro para no escribir ocho iguales). */
#define TAB_CB(nombre, g) static void nombre(button_data_t *d) { (void) d; settings_goto(g); }
TAB_CB(tab_operacion_cb,  GRP_OPERACION)
TAB_CB(tab_audio_cb,      GRP_AUDIO)
TAB_CB(tab_espectro_cb,   GRP_ESPECTRO)
TAB_CB(tab_reloj_cb,      GRP_RELOJ)
TAB_CB(tab_pantalla_cb,   GRP_PANTALLA)
TAB_CB(tab_apariencia_cb, GRP_APARIENCIA)
TAB_CB(tab_acciones_cb,   GRP_ACCIONES)
TAB_CB(tab_info_cb,       GRP_INFO)
#undef TAB_CB

static void tab_mas_cb(button_data_t *d);
static void tab_menos_cb(button_data_t *d);

static button_data_t tab_operacion  = { .type = BTN_TEXT, .label = "Operation",  .press = tab_operacion_cb };
static button_data_t tab_audio      = { .type = BTN_TEXT, .label = "Audio",      .press = tab_audio_cb };
static button_data_t tab_espectro   = { .type = BTN_TEXT, .label = "Spectrum",   .press = tab_espectro_cb };
static button_data_t tab_reloj      = { .type = BTN_TEXT, .label = "Clock",      .press = tab_reloj_cb };
static button_data_t tab_pantalla   = { .type = BTN_TEXT, .label = "Display",    .press = tab_pantalla_cb };
static button_data_t tab_apariencia = { .type = BTN_TEXT, .label = "Appearance", .press = tab_apariencia_cb };
static button_data_t tab_acciones   = { .type = BTN_TEXT, .label = "Actions",    .press = tab_acciones_cb };
static button_data_t tab_info       = { .type = BTN_TEXT, .label = "Info",       .press = tab_info_cb };
static button_data_t tab_mas        = { .type = BTN_TEXT, .label = "More >",     .press = tab_mas_cb };
static button_data_t tab_menos      = { .type = BTN_TEXT, .label = "< More",     .press = tab_menos_cb };

static button_item_t *tabs_items[GRP_COUNT] = {
    &tab_operacion, &tab_audio, &tab_espectro, &tab_reloj,
    &tab_pantalla, &tab_apariencia, &tab_acciones, &tab_info,
};

static buttons_page_t tabs_pagina_1 = {
    .items = { &tab_operacion, &tab_audio, &tab_espectro, &tab_reloj, &tab_mas }
};
static buttons_page_t tabs_pagina_2 = {
    .items = { &tab_pantalla, &tab_apariencia, &tab_acciones, &tab_info, &tab_menos }
};

static void tab_mas_cb(button_data_t *d)   { (void) d; buttons_load_page(&tabs_pagina_2); tabs_marks(); }
static void tab_menos_cb(button_data_t *d) { (void) d; buttons_load_page(&tabs_pagina_1); tabs_marks(); }

/* Deja marcado (LV_STATE_CHECKED) el tab del grupo en el que estas: los del otro lado quedan sin
 * label_obj, asi que buttons_mark los ignora. */
static void tabs_marks(void) {
    for (uint8_t i = 0; i < GRP_COUNT; i++)
        buttons_mark(tabs_items[i], i == grp_actual);
}

/* Primer widget navegable de un subarbol: los ajustes viven dentro de contenedores (la etiqueta y
 * el campo), y al grupo de foco entra el campo, no el contenedor. */
static lv_obj_t *primer_navegable(lv_obj_t *o) {
    if (o == NULL)
        return NULL;

    if (lv_obj_get_group(o) == keyboard_group)
        return o;

    uint32_t hijos = lv_obj_get_child_cnt(o);

    for (uint32_t i = 0; i < hijos; i++) {
        lv_obj_t *r = primer_navegable(lv_obj_get_child(o, i));
        if (r != NULL)
            return r;
    }

    return NULL;
}

/* Salta al grupo: lo lleva ARRIBA de la vista (lv_obj_scroll_to_view solo asegura que se vea) y le
 * da el foco a su primer widget navegable. El foco tiene que viajar con la vista, si no al girar
 * la perilla volveria al widget de antes. */
static void settings_goto(uint8_t g) {
    if (g >= GRP_COUNT || grid == NULL)
        return;

    grp_actual = g;

    lv_obj_t *primero = lv_obj_get_child(grid, grp_primero[g]);
    if (primero == NULL)
        return;

    lv_obj_scroll_to_y(grid, lv_obj_get_y(primero), LV_ANIM_OFF);

    for (uint16_t i = grp_primero[g]; i < grp_ultimo[g]; i++) {
        lv_obj_t *nav = primer_navegable(lv_obj_get_child(grid, i));
        if (nav != NULL) {
            lv_group_focus_obj(nav);
            break;
        }
    }

    tabs_marks();
}

static dialog_t     dialog = {
    .run = false,
    .btn_page = &tabs_pagina_1,
    .construct_cb = construct_cb,
    .destruct_cb = NULL,
    .audio_cb = NULL,
    .key_cb = key_cb
};

dialog_t            *dialog_settings = &dialog;

/* Shared update */

static void anunciar_ajuste(lv_obj_t *widget, const char *valor) {
    lv_obj_t *celda = widget != NULL ? lv_obj_get_parent(widget) : NULL;
    lv_obj_t *nombre = NULL;

    if (valor == NULL) {
        return;
    }

    /* El nombre del ajuste es una etiqueta de la celda; se busca entre los hijos en lugar de
     * suponer el indice, porque el orden cambia segun el widget. */
    if (celda != NULL) {
        for (uint32_t i = 0; i < 8 && nombre == NULL; i++) {
            lv_obj_t *hijo = lv_obj_get_child(celda, i);

            if (hijo == NULL) {
                break;
            }

            if (hijo != widget && lv_obj_get_class(hijo) == &lv_label_class) {
                nombre = hijo;
            }
        }
    }

    if (nombre != NULL) {
        voice_say_text_fmt("%s: %s", lv_label_get_text(nombre), valor);
    } else {
        /* Sin nombre igual se dice el valor: mejor eso que nada */
        LV_LOG_USER("Ajuste sin etiqueta, se anuncia solo el valor: %s", valor);
        voice_say_text_fmt("%s", valor);
    }
}

static void bool_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    params_bool_t   *var = lv_event_get_user_data(e);
    anunciar_ajuste(obj, lv_obj_has_state(obj, LV_STATE_CHECKED) ? "on" : "off");

    params_bool_set(var, lv_obj_has_state(obj, LV_STATE_CHECKED));
}

static void uint8_spinbox_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    params_uint8_t  *var = lv_event_get_user_data(e);
    char valor[16];
    snprintf(valor, sizeof(valor), "%i", (int) lv_spinbox_get_value(obj));
    anunciar_ajuste(obj, valor);

    params_uint8_set(var, lv_spinbox_get_value(obj));
}

static void uint8_dropdown_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    params_uint8_t  *var = lv_event_get_user_data(e);
    anunciar_ajuste(obj, lv_dropdown_get_text(obj));

    params_uint8_set(var, lv_dropdown_get_selected(obj));
}

/* Los temas se aplican al arrancar: cada ventana toma sus colores cuando se construye, asi que
 * se guarda el nombre y la GUI se reinicia (el x6200_daemon la vuelve a lanzar), igual que el
 * X6100. La lista sale de theme_list(): los seis temas v2 (#73). */
static void restart_cb(lv_timer_t *t) {
    (void) t;
    exit(0);        /* el x6200_daemon la vuelve a lanzar con el tema nuevo */
}

static void theme_update_cb(lv_event_t * e) {
    char name[32];

    lv_dropdown_get_selected_str(lv_event_get_target(e), name, sizeof(name));
    params_str_set(&params.theme_name, name);
    params_flush();
    msg_update_text_fmt("Theme: %s - restarting", name);

    lv_timer_t *restart = lv_timer_create(restart_cb, 1200, NULL);
    lv_timer_set_repeat_count(restart, 1);
}

/* Shared create */

static lv_obj_t * switch_bool(lv_obj_t *parent, params_bool_t *var) {
    lv_obj_t *obj = lv_switch_create(parent);
    styles_switch_init(obj);

    dialog_item(&dialog, obj);

    lv_obj_center(obj);
    lv_obj_add_event_cb(obj, bool_update_cb, LV_EVENT_VALUE_CHANGED, var);

    if (var->x) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    }

    return obj;
}

static lv_obj_t * spinbox_uint8(lv_obj_t *parent, params_uint8_t *var) {
    lv_obj_t *obj = lv_spinbox_create(parent);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, var->x);
    lv_spinbox_set_range(obj, var->min, var->max);
    lv_obj_add_event_cb(obj, uint8_spinbox_update_cb, LV_EVENT_VALUE_CHANGED, var);

    return obj;
}

static lv_obj_t * dropdown_uint8_custom_cb(lv_obj_t *parent, params_uint8_t *var, const char *options, lv_event_cb_t cb) {
    lv_obj_t *obj = lv_dropdown_create(parent);

    dialog_item(&dialog, obj);

    lv_obj_add_event_cb(obj, cb, LV_EVENT_VALUE_CHANGED, var);

    lv_obj_t *list = lv_dropdown_get_list(obj);
    lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

    lv_dropdown_set_options(obj, options);
    lv_dropdown_set_symbol(obj, NULL);

    lv_dropdown_set_selected(obj, var->x);

    return obj;
}

static lv_obj_t * dropdown_uint8(lv_obj_t *parent, params_uint8_t *var, const char *options) {
    return dropdown_uint8_custom_cb(parent, var, options, uint8_dropdown_update_cb);
}

static void audio_enh_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    params_uint8_t  *var = lv_event_get_user_data(e);

    params_uint8_set(var, lv_dropdown_get_selected(obj));
    audio_enh_preset_load((audio_enh_preset_t) var->x);
}

static void waterfall_battery_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    waterfall_set_battery_mode(lv_obj_has_state(obj, LV_STATE_CHECKED));
}

static void slider_with_text(lv_obj_t *cell, int32_t val, int32_t min, int32_t max, size_t width, char *fmt, lv_event_cb_t event_cb) {
    lv_obj_t *obj;
    obj = lv_slider_create(cell);
    styles_slider_init(obj);

    dialog_item(&dialog, obj);

    lv_slider_set_mode(obj, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_range(obj, min, max);
    lv_slider_set_value(obj, val, LV_ANIM_OFF);
    lv_obj_set_width(obj, width);

    /*Create a label below the slider*/
    lv_obj_t *slider_label = lv_label_create(cell);
    lv_obj_set_user_data(slider_label, fmt);
    /* La etiqueta se escribe con el mismo `fmt` que el callback usa despues, asi que el
     * formato decide con que tipo se pasa el valor por varargs: "%d" lleva un entero y "%.1f"
     * un double. El valor de un slider de ganancia es un float y llegaba como entero, que por
     * varargs es el tipo equivocado: la etiqueta de "Play,Rec gain" quedaba con basura (no la
     * del valor guardado) hasta que el usuario la movia (fila del issue #26). */
    if (strchr(fmt, '.') != NULL) {
        lv_label_set_text_fmt(slider_label, fmt, (double) val);
    } else {
        lv_label_set_text_fmt(slider_label, fmt, val);
    }
    lv_obj_align(slider_label, LV_ALIGN_RIGHT_MID, 12, 0);
    lv_obj_set_style_text_color(slider_label, TC(text), 0);

    lv_obj_add_event_cb(obj, event_cb, LV_EVENT_VALUE_CHANGED, slider_label);
}

/* Datetime */

static void datetime_update_cb(lv_event_t * e) {
    ts.tm_mday = lv_spinbox_get_value(day);
    ts.tm_mon = lv_spinbox_get_value(month) - 1;
    ts.tm_year = lv_spinbox_get_value(year) - 1900;
    ts.tm_hour = lv_spinbox_get_value(hour);
    ts.tm_min = lv_spinbox_get_value(min);
    ts.tm_sec = lv_spinbox_get_value(sec);

    /* Set system */

    struct timespec tp;

    tp.tv_sec = mktime(&ts);
    tp.tv_nsec = 0;

    int res = clock_settime(CLOCK_REALTIME, &tp);
    if (res != 0)
    {
        LV_LOG_ERROR("Can't set system time: %s\n", strerror(errno));
        return;
    }

}

static void datetime_set_rtc_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    if (!lv_group_get_editing(lv_obj_get_group(obj)))
    {
        /* Set RTC */

        int rtc = open("/dev/rtc1", O_WRONLY);

        if (rtc > 0) {
            int res = ioctl(rtc, RTC_SET_TIME, &ts);
            if (res != 0)
            {
                LV_LOG_ERROR("Can't set RTC time: %s\n", strerror(errno));
                return;
            }
            close(rtc);
        } else {
            LV_LOG_ERROR("Can't open /dev/rtc1: %s\n", strerror(errno));
        }
    }

}

static uint8_t make_date(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    /* Label */

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Day, Month, Year");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* Day */

    obj = lv_spinbox_create(grid);
    day = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_mday);
    lv_spinbox_set_range(obj, 1, 31);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    /* Month */

    obj = lv_spinbox_create(grid);
    month = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_mon + 1);
    lv_spinbox_set_range(obj, 1, 12);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    /* Year */

    obj = lv_spinbox_create(grid);
    year = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_year + 1900);
    lv_spinbox_set_range(obj, 2020, 2038);
    lv_spinbox_set_digit_format(obj, 4, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    /* El año lleva 4 cifras y es el tercer campo de la fila: tiene que verse como Dia y Mes.
     * Los tres comparten la caja de dos columnas de la grilla (SMALL_2 = 93) y el padding
     * horizontal de la grilla (SMALL_PAD = 3, el que separa las columnas), asi que la caja
     * util es 93 - 2*3 (pad) - 2*2 (borde) = 83 px. Las 4 cifras miden 60 con la fuente del
     * tema (sony_36, 15 px por digito): el label "2026" de la fixture settings-home mide
     * 696,113 83x28 con natural 60 (medido con --inspect), asi que entra con 23 px de margen,
     * sin recorte (el peor caso de 4 cifras, "0000", tambien mide 60).
     * Antes el ancho se media por contenido (el helper `ancho_campo_cifras`, el fix de UI-016):
     * 60 (texto) + 6 (pad) + 4 (borde) = 70 contra los 93 de Dia y Mes, asi que este campo
     * quedaba 23 px mas angosto (issue #110, medido en la misma fixture: spinbox 691,100 70x56
     * vs 499,100/595,100 93x56). Un spinbox no edita texto libre: se le deja el padding de la
     * grilla y la caja de la fila. */
    lv_obj_set_style_pad_hor(obj, SMALL_PAD, LV_PART_MAIN);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    return row + 1;
}

static uint8_t make_time(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    /* Label */

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Hour, Min, Sec");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* Hour */

    obj = lv_spinbox_create(grid);
    hour = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_hour);
    lv_spinbox_set_range(obj, 0, 23);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    /* Min */

    obj = lv_spinbox_create(grid);
    min = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_min);
    lv_spinbox_set_range(obj, 0, 59);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    /* Sec */

    obj = lv_spinbox_create(grid);
    sec = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_sec);
    lv_spinbox_set_range(obj, 0, 59);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    return row + 1;
}

/* Backlight */

static void backlight_timeout_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    params_lock();
    params.brightness_timeout = lv_spinbox_get_value(obj);
    params_unlock(&params.dirty.brightness_timeout);

    backlight_tick();
}

static void backlight_brightness_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    params_lock();
    params.brightness_normal = lv_slider_get_value(obj);
    params_unlock(&params.dirty.brightness_normal);

    params_lock();
    params.brightness_idle = lv_slider_get_left_value(obj);
    params_unlock(&params.dirty.brightness_idle);

    /* El preview del slider pasa por la MISMA puerta que el resto del brillo
     * (UI-08 incr. 5): sin esto el arrastre aplicaba el setting crudo y saltaba
     * el cap del perfil vigente. El setting (params.brightness_normal) se guarda
     * entero: el cap solo recorta el nivel que va al panel, nunca el parametro. */
    visual_policy_brightness_rewrite(params.brightness_normal);
}

static void backlight_buttons_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    backlight_set_buttons(lv_dropdown_get_selected(obj));
}

static uint8_t make_backlight(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    /* Label */

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Timeout, Brightness");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* Timeout */

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, params.brightness_timeout);
    lv_spinbox_set_range(obj, 5, 120);
    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, backlight_timeout_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Brightness */

    obj = lv_obj_create(grid);

    /* La unica excepcion del esquema (ver el bloque del encabezado): el slider de brillo es de
     * rango y mide SMALL_4-30=159, asi que su caja usa la mitad ancha (span 4) y arranca en la
     * columna 3 -una columna del mismo esquema-, no en la 4. */
    lv_obj_set_size(obj, SMALL_4, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_BRILLO, SPAN_BRILLO, LV_GRID_ALIGN_CENTER, row, 1);   col += SPAN_BRILLO;
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = lv_slider_create(obj);
    styles_slider_init(obj);

    dialog_item(&dialog, obj);

    lv_slider_set_mode(obj, LV_SLIDER_MODE_RANGE);
    lv_slider_set_value(obj, params.brightness_normal, LV_ANIM_OFF);
    lv_slider_set_left_value(obj, params.brightness_idle, LV_ANIM_OFF);
    lv_slider_set_range(obj, -1, 9);
    lv_obj_set_width(obj, SMALL_4 - 30);
    lv_obj_center(obj);

    lv_obj_add_event_cb(obj, backlight_brightness_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    row++;
    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Buttons brightness");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_dropdown_create(grid);

    dialog_item(&dialog, obj);

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    lv_obj_t *list = lv_dropdown_get_list(obj);
    lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

    lv_dropdown_set_options(obj, " Always Off \n Always On \n Temporarily On ");
    lv_dropdown_set_symbol(obj, NULL);
    lv_dropdown_set_selected(obj, params.brightness_buttons);
    lv_obj_add_event_cb(obj, backlight_buttons_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return row + 1;
}

/* Perfil visual de bateria (UI-08 incr. 5). El desplegable elige el perfil
 * SETEADO (AUTO o uno fijo) y lo persiste en params.visual_profile con el mismo
 * lock/dirty que las filas vecinas; la etiqueta muestra ademas el perfil
 * VIGENTE, que puede diferir del seteado cuando decide la bateria (AUTO). */

/* Indice del desplegable para un valor de params: 0 = AUTO, 1..VP_PROFILE_COUNT
 * = el perfil (profile + 1). Un valor desconocido cae a AUTO, el mismo fallback
 * que visual_policy_override_from_param(). */
static uint8_t visual_profile_selection(uint8_t value) {
    return (value < VP_PROFILE_COUNT) ? (uint8_t) (value + 1) : 0;
}

static void visual_profile_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    uint8_t   selected = (uint8_t) lv_dropdown_get_selected(obj);

    /* 0 = AUTO (el valor reservado); 1.. = el perfil selected - 1.
     * params_uint8_set toma el lock y marca el dirty, como el resto de los
     * ajustes; la politica lo lee (radio_tick, src/radio.c) pero nunca lo toca. */
    params_uint8_set(&params.visual_profile,
                     selected == 0 ? VP_PARAM_AUTO : (uint8_t) (selected - 1));
}

static uint8_t make_visual_profile(uint8_t row) {
    lv_obj_t    *obj;

    row_dsc[row] = 54;

    /* La etiqueta nombra el ajuste y, entre parentesis, el perfil VIGENTE: en
     * AUTO lo decide la politica (y en manual lo congela el override), asi que
     * no siempre coincide con lo que muestra el desplegable. Es una foto del
     * estado al abrir Ajustes. */
    obj = lv_label_create(grid);
    lv_label_set_text_fmt(obj, "Visual profile (%s)",
                          visual_policy_profile_name(visual_policy_current()));
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_dropdown_create(grid);

    dialog_item(&dialog, obj);

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    lv_obj_t *list = lv_dropdown_get_list(obj);
    lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

    lv_dropdown_set_options(obj, " Auto \n Quality \n Balanced \n Eco \n Critical ");
    lv_dropdown_set_symbol(obj, NULL);
    lv_dropdown_set_selected(obj, visual_profile_selection(params.visual_profile.x));
    lv_obj_add_event_cb(obj, visual_profile_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return row + 1;
}

/* Line-in, Line-out */

static void line_in_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *slider_label = (lv_obj_t *)lv_event_get_user_data(e);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    int32_t val = lv_slider_get_value(obj);
    radio_set_line_in(val);
    lv_label_set_text_fmt(slider_label, fmt, val);
}

static void line_out_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *slider_label = (lv_obj_t *)lv_event_get_user_data(e);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    int32_t val = lv_slider_get_value(obj);
    radio_set_line_out(val);
    lv_label_set_text_fmt(slider_label, fmt, val);
}

static uint8_t make_line_gain(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;

    row_dsc[row] = 54;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "Line-in, Line-out");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_2A, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, params.line_in, 0, 30, SMALL_3 - 30 - 60, "%d", line_in_update_cb);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, COL_2B, SPAN_2B, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, params.line_out, 0, 30, SMALL_3 - 30 - 60, "%d", line_out_update_cb);

    return row + 1;
}

/* Mag Freq, Info, ALC */

static uint8_t make_mag(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Mag Freq, Info, ALC");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* Freq */

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.mag_freq);

    lv_obj_set_width(obj, SMALL_2 - 30);

    /* Info */

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.mag_info);

    lv_obj_set_width(obj, SMALL_2 - 30);

    /* ALC */

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.mag_alc);

    lv_obj_set_width(obj, SMALL_2 - 30);

    return row + 1;
}

/* Clock */

static void clock_view_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    clock_set_view(lv_dropdown_get_selected(obj));
}

static void clock_time_timeout_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    clock_set_time_timeout(lv_spinbox_get_value(obj));
}

static void clock_power_timeout_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    clock_set_power_timeout(lv_spinbox_get_value(obj));
}

static void clock_tx_timeout_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    clock_set_tx_timeout(lv_spinbox_get_value(obj));
}

static uint8_t make_smeter(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "S-meter needle");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.smeter_needle);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static void band_plan_refresh_cb(lv_event_t * e) {
    (void) e;

    /* The parameter itself is handled by switch_bool(); here we only redraw. */
    band_plan_refresh();
}

static uint8_t make_band_plan(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Band plan");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.band_plan);

    lv_obj_add_event_cb(obj, band_plan_refresh_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static void wfview_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    wfview_server_set_enabled(lv_obj_has_state(obj, LV_STATE_CHECKED));
}

static uint8_t make_wfview(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "wfview server");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    /* The switch mirrors /mnt/wfview.conf (enabled=), which the web
     * configurator can also rewrite; credentials live in that file. */
    obj = lv_switch_create(obj);
    styles_switch_init(obj);
    dialog_item(&dialog, obj);
    lv_obj_center(obj);
    lv_obj_add_event_cb(obj, wfview_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (wfview_server_get_enabled()) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    }

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static uint8_t make_clock(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Clock view");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_dropdown_create(grid);

    dialog_item(&dialog, obj);

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    lv_obj_t *list = lv_dropdown_get_list(obj);
    lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

    lv_dropdown_set_options(obj, " Always Time \n Time and Power \n Always Power");
    lv_dropdown_set_symbol(obj, NULL);
    lv_dropdown_set_selected(obj, params.clock_view);
    lv_obj_add_event_cb(obj, clock_view_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* * */

    row++;
    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Timeout Clock, Power, TX");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, params.clock_time_timeout);
    lv_spinbox_set_range(obj, 1, 59);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, clock_time_timeout_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, params.clock_power_timeout);
    lv_spinbox_set_range(obj, 1, 59);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, clock_power_timeout_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, params.clock_tx_timeout);
    lv_spinbox_set_range(obj, 0, 10);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, clock_tx_timeout_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return row + 1;
}

/* Long press actions */

typedef struct {
    char                *label;
    press_action_t      action;
} action_items_t;

static action_items_t long_action_items[] = {
    { .label = " None ", .action = ACTION_NONE },
    { .label = " Screenshot ", .action = ACTION_SCREENSHOT },
    { .label = " Screen rec ", .action = ACTION_SCREEN_REC },
    { .label = " Recorder on/off ", .action = ACTION_RECORDER },
    { .label = " Mute ", .action = ACTION_MUTE },
    { .label = " Voice mode ", .action = ACTION_VOICE_MODE },
    { .label = " ATT ", .action = ACTION_ATT_TOGGLE },
    { .label = " PRE ", .action = ACTION_PRE_TOGGLE },
    { .label = " ATU ", .action = ACTION_ATU_TOGGLE },
    { .label = " ATU tune ", .action = ACTION_ATU_TUNE },
    /* "WF mode" (waterfall fijo/centrado) todavia NO esta portado: el equipo no tiene ese
     * parametro y su waterfall.c es otra version. La accion existe para cuando se porte,
     * pero no se ofrece, para no dejar un boton que no hace nada (lo vigila check_actions.py). */
    { .label = " APP Bands ", .action = ACTION_APP_BANDS },
    { .label = " Battery info ", .action = ACTION_BAT_INFO },
    { .label = " APP RTTY ", .action = ACTION_APP_RTTY },
    { .label = " APP Callsign", .action = ACTION_APP_CALLSIGN },
    { .label = " APP WiFi", .action = ACTION_APP_WIFI },
    { .label = " APP EQ", .action = ACTION_APP_EQ },
    /* Editor DFL (issue #173, PR 2): abre el editor de filtro desde el long press
     * fisico DFL. "Battery info" sigue estando en la lista. */
    { .label = " APP DFL editor", .action = ACTION_APP_DFL_EDITOR },
    { .label = " APP FT8 ", .action = ACTION_APP_FT8 },
    { .label = " APP SWR Scan ", .action = ACTION_APP_SWRSCAN },
    { .label = " APP Settings", .action = ACTION_APP_SETTINGS },
    { .label = " APP Recorder", .action = ACTION_APP_RECORDER },
    { .label = " QTH Grid", .action = ACTION_APP_QTH },
    /* Apps ported from the X6100 firmware (docs/NEW_FEATURES.md) */
    { .label = " APP WSPR", .action = ACTION_APP_WSPR },
    { .label = " APP NavTex", .action = ACTION_APP_NAVTEX },
    { .label = " APP SSTV", .action = ACTION_APP_SSTV },
    { .label = " APP WeFax", .action = ACTION_APP_WEFAX },
    { .label = " APP JS8", .action = ACTION_APP_JS8 },
    { .label = " APP Cluster", .action = ACTION_APP_CLUSTER },
    { .label = " APP Field Ops", .action = ACTION_APP_FIELD },
    { .label = " APP GPS", .action = ACTION_APP_GPS },
    { .label = " APP CW Keyboard", .action = ACTION_APP_CW_KBD },
    { .label = " APP CW Trainer", .action = ACTION_APP_CWTRAINER },
    { .label = " APP Bluetooth", .action = ACTION_APP_BT },
    { .label = " APP Status", .action = ACTION_APP_STATUS },
    { .label = " APP Propagation", .action = ACTION_APP_PROPAGATION },
    { .label = " Theme", .action = ACTION_THEME },
    { .label = NULL, .action = ACTION_NONE }
};

static void long_action_update_cb(lv_event_t * e) {
    lv_obj_t    *obj = lv_event_get_target(e);
    uint32_t    *i = lv_event_get_user_data(e);
    uint8_t     val = long_action_items[lv_dropdown_get_selected(obj)].action;

    params_lock();

    switch (*i) {
        case 0:
            params.long_gen = val;
            params_unlock(&params.dirty.long_gen);
            break;

        case 1:
            params.long_app = val;
            params_unlock(&params.dirty.long_app);
            break;

        case 2:
            params.long_key = val;
            params_unlock(&params.dirty.long_key);
            break;

        case 3:
            params.long_msg = val;
            params_unlock(&params.dirty.long_msg);
            break;

        case 4:
            params.long_dfn = val;
            params_unlock(&params.dirty.long_dfn);
            break;

        case 5:
            params.long_dfl = val;
            params_unlock(&params.dirty.long_dfl);
            break;
    }
}

static uint8_t make_long_action(uint8_t row) {
    char        *labels[] = { "GEN long press", "APP long press", "KEY long press", "MSG long press", "DFN long press", "DFL long press" };
    lv_obj_t    *obj;

    for (uint8_t i = 0; i < 6; i++) {
        row_dsc[row] = 54;

        obj = lv_label_create(grid);

        lv_label_set_text(obj, labels[i]);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

        obj = lv_dropdown_create(grid);

        dialog_item(&dialog, obj);

        lv_obj_set_size(obj, SMALL_6, 56);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
        lv_obj_center(obj);

        lv_obj_t *list = lv_dropdown_get_list(obj);
        lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

        lv_dropdown_set_symbol(obj, NULL);

        uint8_t x;

        switch (i) {
            case 0: x = params.long_gen;    break;
            case 1: x = params.long_app;    break;
            case 2: x = params.long_key;    break;
            case 3: x = params.long_msg;    break;
            case 4: x = params.long_dfn;    break;
            case 5: x = params.long_dfl;    break;

            default:
                x = ACTION_NONE;
                break;
        }

        lv_dropdown_clear_options(obj);

        uint8_t n = 0;

        while (long_action_items[n].label) {
            lv_dropdown_add_option(obj, long_action_items[n].label, LV_DROPDOWN_POS_LAST);

            if (long_action_items[n].action == x) {
                lv_dropdown_set_selected(obj, n);
            }

            n++;
        }

        uint32_t *param = malloc(sizeof(uint32_t));
        *param = i;

        lv_obj_add_event_cb(obj, long_action_update_cb, LV_EVENT_VALUE_CHANGED, param);

        row++;
    }

    return row;
}

/* HMic F1, F2 actions */

static action_items_t hmic_action_items[] = {
    { .label = " None ", .action = ACTION_NONE },
    { .label = " Recorder on/off ", .action = ACTION_RECORDER },
    { .label = " Mute ", .action = ACTION_MUTE },
    { .label = " Step up ", .action = ACTION_STEP_UP },
    { .label = " Step down ", .action = ACTION_STEP_DOWN },
    { .label = " Voice mode ", .action = ACTION_VOICE_MODE },
    { .label = " Battery info ", .action = ACTION_BAT_INFO },
    { .label = " NR toggle ", .action = ACTION_NR_TOGGLE },
    { .label = " NB toggle ", .action = ACTION_NB_TOGGLE },
    { .label = NULL, .action = ACTION_NONE }
};

static void hmic_action_update_cb(lv_event_t * e) {
    lv_obj_t    *obj = lv_event_get_target(e);
    uint8_t     *i = lv_event_get_user_data(e);
    uint8_t     val = hmic_action_items[lv_dropdown_get_selected(obj)].action;

    params_lock();

    switch (*i) {
        case 0:
            params.press_f1 = val;
            params_unlock(&params.dirty.press_f1);
            break;

        case 1:
            params.press_f2 = val;
            params_unlock(&params.dirty.press_f2);
            break;

        case 2:
            params.long_f1 = val;
            params_unlock(&params.dirty.long_f1);
            break;

        case 3:
            params.long_f2 = val;
            params_unlock(&params.dirty.long_f2);
            break;
    }
}

static uint8_t make_hmic_action(uint8_t row) {
    struct {char *label; uint8_t param;} items[] = {
        {"HMic F1 press", params.press_f1},
        {"HMic F2 press", params.press_f2},
        {"HMic F1 long press", params.long_f1},
        {"HMic F2 long press", params.long_f2}
    };
    size_t items_len = sizeof(items) / sizeof(items[0]);
    lv_obj_t    *obj;

    for (uint8_t i = 0; i < items_len; i++) {
        row_dsc[row] = 54;

        obj = lv_label_create(grid);

        lv_label_set_text(obj, items[i].label);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

        obj = lv_dropdown_create(grid);

        dialog_item(&dialog, obj);

        lv_obj_set_size(obj, SMALL_6, 56);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
        lv_obj_center(obj);

        lv_obj_t *list = lv_dropdown_get_list(obj);
        lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

        lv_dropdown_set_symbol(obj, NULL);

        lv_dropdown_clear_options(obj);

        uint8_t n = 0;

        while (hmic_action_items[n].label) {
            lv_dropdown_add_option(obj, hmic_action_items[n].label, LV_DROPDOWN_POS_LAST);

            if (hmic_action_items[n].action == items[i].param) {
                lv_dropdown_set_selected(obj, n);
            }

            n++;
        }

        uint8_t *param = malloc(sizeof(uint8_t));
        *param = i;

        lv_obj_add_event_cb(obj, hmic_action_update_cb, LV_EVENT_VALUE_CHANGED, param);

        row++;
    }

    return row;
}

/* Play,Rec gain
 *
 * Cada slider mueve SU ganancia y guarda SU parametro: el de la izquierda la salida
 * (audio_set_play_vol -> params.play_gain_db_f) y el de la derecha la entrada
 * (audio_set_rec_vol -> params.rec_gain_db_f). Los dos sliders se ven iguales, y el
 * callback de Rec salio copiado del de Play llamando a la funcion de salida (issue #26):
 * arrastrarlo movia el volumen que se escucha y el gain de entrada no se aplicaba nunca.
 * tests/check_settings_focus.py lo comprueba (el parametro del slider tiene que aparecer
 * en el cuerpo de su callback), asi que un copiar/pegar cruzado lo deja en rojo. */

static void play_gain_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    float val = audio_set_play_vol(lv_slider_get_value(obj));

    params_float_set(&params.play_gain_db_f, val);

    lv_obj_t *slider_label = (lv_obj_t *)lv_event_get_user_data(e);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);
}

static void rec_gain_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    float val = audio_set_rec_vol(lv_slider_get_value(obj));

    params_float_set(&params.rec_gain_db_f, val);
    lv_obj_t *slider_label = (lv_obj_t *)lv_event_get_user_data(e);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);
}

static uint8_t make_audio_gain(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;

    row_dsc[row] = 54;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "Play,Rec gain");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_2A, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, params.play_gain_db_f.x, -10, 10, SMALL_3 - 30 - 65, "%.1f", play_gain_update_cb);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, COL_2B, SPAN_2B, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, params.rec_gain_db_f.x, -10, 10, SMALL_3 - 30 - 65, "%.1f", rec_gain_update_cb);

    return row + 1;
}

/* Audio enhancement (AF leveler / NR presets, see docs/AUDIO_ENHANCEMENT_GUIDE.md) */

static uint8_t make_audio_enh(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Audio enhance");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = dropdown_uint8_custom_cb(grid, &params.audio_enh_preset,
                                   " Off \n CW weak \n SSB voice \n AM broadcast \n Digital",
                                   audio_enh_update_cb);

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    return row + 1;
}

/* Transverter */

static void transverter_from_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    cfg_transverter_t   *transverter = lv_event_get_user_data(e);
    subject_set_int(transverter->from.val, lv_spinbox_get_value(obj) * 1000000L);
}

static void transverter_to_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    cfg_transverter_t   *transverter = lv_event_get_user_data(e);
    subject_set_int(transverter->to.val, lv_spinbox_get_value(obj) * 1000000L);
}

static void transverter_shift_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    cfg_transverter_t   *transverter = lv_event_get_user_data(e);
    subject_set_int(transverter->shift.val, lv_spinbox_get_value(obj) * 1000000L);
}

static uint8_t make_transverter(uint8_t row, uint8_t n) {
    lv_obj_t        *obj;
    uint8_t         col = 0;
    cfg_transverter_t   *transverter = &cfg_transverters[n];

    /* Label */

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text_fmt(obj, "Transverter %i", n + 1);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* From */

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, subject_get_int(transverter->from.val) / 1000000L);
    lv_spinbox_set_range(obj, 70, 500);
    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, transverter_from_update_cb, LV_EVENT_VALUE_CHANGED, transverter);

    /* To */

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, subject_get_int(transverter->to.val) / 1000000L);
    lv_spinbox_set_range(obj, 70, 500);
    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, transverter_to_update_cb, LV_EVENT_VALUE_CHANGED, transverter);

    /* Shift */

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, subject_get_int(transverter->shift.val) / 1000000L);
    lv_spinbox_set_range(obj, 42, 500);
    lv_spinbox_set_digit_format(obj,3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, transverter_shift_update_cb, LV_EVENT_VALUE_CHANGED, transverter);

    return row + 1;
}

/* Voice */

static uint8_t make_voice(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Voice mode");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = dropdown_uint8(grid, &params.voice_mode, " Off \n When LCD off \n Always");

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    /* * */

    row++;
    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Voice rate, pitch, volume");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = spinbox_uint8(grid, &params.voice_rate);

    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;

    obj = spinbox_uint8(grid, &params.voice_pitch);

    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;

    obj = spinbox_uint8(grid, &params.voice_volume);

    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, SPAN_3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;

    return row + 1;
}

/* Spectrum and waterfall auto */

static uint8_t make_auto(uint8_t row) {
    lv_obj_t    *obj;

    row_dsc[row] = 54;

    /* Spectrum */

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Spectrum auto min, max");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_2A, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.spectrum_auto_min);

    lv_obj_set_width(obj, SMALL_3 - 30);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_2B, SPAN_2B, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.spectrum_auto_max);

    lv_obj_set_width(obj, SMALL_3 - 30);

    /* Waterfall */

    row++;
    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Waterfall auto min, max");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_2A, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.waterfall_auto_min);

    lv_obj_set_width(obj, SMALL_3 - 30);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_2B, SPAN_2B, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.waterfall_auto_max);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static uint8_t make_waterfall_line_zoom(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Waterfall line, zoom");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_2A, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.waterfall_center_line);

    lv_obj_set_width(obj, SMALL_3 - 30);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_2B, SPAN_2B, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.waterfall_zoom);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static uint8_t make_waterfall_smooth_scroll(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Waterfall smooth scroll");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.waterfall_smooth_scroll);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static uint8_t make_waterfall_battery(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Waterfall battery mode");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.waterfall_battery);

    lv_obj_add_event_cb(obj, waterfall_battery_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static void sp_mode_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
        radio_change_spmode(-1);
    } else {
        radio_change_spmode(1);
    }
}

static uint8_t make_sp_mode(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Speaker mode");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = lv_switch_create(obj);
    styles_switch_init(obj);

    dialog_item(&dialog, obj);

    lv_obj_center(obj);
    lv_obj_add_event_cb(obj, sp_mode_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (!params.spmode.x) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    }

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static uint8_t make_freq_accel(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Freq acceleration");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = dropdown_uint8(grid, &params.freq_accel, " None \n Lite \n Strong");

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    return row + 1;
}

static uint8_t make_theme(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Theme");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* La oferta del selector: los seis temas v2 (theme_list(), src/theme.c, #73).
     * Un tema guardado que no esté en la lista (un legado, hasta la migración del
     * #74) no se encuentra y el desplegable cae a la primera opción. */
    {
        char        names[16][32];
        char        opciones[16 * 34];
        int         n     = theme_list(names, 16);
        int         sel   = 0;
        size_t      usado = 0;

        opciones[0] = 0;

        for (int i = 0; i < n; i++) {
            int escrito = snprintf(opciones + usado, sizeof(opciones) - usado, "%s%s",
                                   i ? "\n" : "", names[i]);

            if (escrito < 0 || (size_t) escrito >= sizeof(opciones) - usado) {
                break;
            }

            usado += (size_t) escrito;

            if (strcasecmp(names[i], params.theme_name.x) == 0) {
                sel = i;
            }
        }

        obj = lv_dropdown_create(grid);

        /* dialog_item() es lo que lo mete en el grupo de foco: sin esto el desplegable no se
         * puede seleccionar desde Ajustes (los demas desplegables si lo llaman). */
        dialog_item(&dialog, obj);

        lv_dropdown_set_options(obj, opciones);
        lv_dropdown_set_selected(obj, sel);
        lv_obj_add_event_cb(obj, theme_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    return row + 1;
}

/* The font family is swapped once at start (fonts_apply()), like the X6100 */
static void font_update_cb(lv_event_t * e) {
    uint8_dropdown_update_cb(e);
    msg_update_text_fmt("Font: from the next start");
}

static uint8_t make_font(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Font");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* The item list comes from the generated font_names[] (fonts/font_families.h,
     * via font_select.h): the row that is picked IS the FONT_* value, so this and
     * fonts_apply() cannot drift apart (see src/main.c). */
    obj = dropdown_uint8_custom_cb(grid, &params.font, font_dropdown_items(), font_update_cb);

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    return row + 1;
}

/* --- Bluetooth (btcat) ----------------------------------------------------
 * Pairing is done from the console with btaudio-ctl / btcat-setup, exactly as in
 * the X6100; these are its power and discoverable switches, which run the same
 * btcat-ctl the scripts use. */
#define BTCAT_CTL   "/usr/sbin/btcat-ctl"

static lv_obj_t *bt_visible_sw;


static void *btcat_run_thread(void *arg) {
    char *cmd = arg;

    int rc = system(cmd);

    (void) rc;
    free(cmd);

    return NULL;
}

/* btcat-ctl talks to bluetoothd: run it off the UI thread */
static void btcat_run(const char *what, bool on) {
    char *cmd = malloc(strlen(BTCAT_CTL) + strlen(what) + 24);

    if (!cmd) {
        return;
    }

    sprintf(cmd, "%s %s %s >/dev/null 2>&1", BTCAT_CTL, what, on ? "on" : "off");

    pthread_t t;

    if (pthread_create(&t, NULL, btcat_run_thread, cmd) == 0) {
        pthread_detach(t);
    } else {
        free(cmd);
    }
}

static void bt_power_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    bool      on  = lv_obj_has_state(obj, LV_STATE_CHECKED);

    btcat_run("bt", on);

    if (bt_visible_sw) {
        if (on) {
            if (bt_visible_sw) lv_obj_clear_state(bt_visible_sw, LV_STATE_DISABLED);
        } else {
            if (bt_visible_sw) lv_obj_add_state(bt_visible_sw, LV_STATE_DISABLED);
        }
    }
}

static void bt_visible_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    btcat_run("discoverable", lv_obj_has_state(obj, LV_STATE_CHECKED));
}

static void bt_visible_delete_cb(lv_event_t * e) {
    (void) e;

    bt_visible_sw = NULL;
}

/* Abre la app de dispositivos Bluetooth (dialog_bt): ahi se escanea, se ve la lista de pareados y
 * de encontrados, se elige a cual emparejarse, se conecta/desconecta y se muestra el passkey. Hasta
 * ahora esa app solo se podia abrir asignandole ACTION_APP_BT a un long press (docs/HALLAZGOS.md 20);
 * el propio encabezado de dialog_bt.c dice "Settings -> Bluetooth", asi que el lugar es este. Se
 * activa tocando la fila o con ENTER/MFK sobre ella. */
static void bt_devices_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        main_screen_start_app(ACTION_APP_BT);
    } else if (code == LV_EVENT_KEY) {
        if (*((uint32_t *) lv_event_get_param(e)) == LV_KEY_ENTER) {
            main_screen_start_app(ACTION_APP_BT);
        }
    }
}

/* params.charger is an enum, not a params_uint8_t: drive the dropdown by hand */
static void charger_update_cb(lv_event_t * e) {
    params.charger = (radio_charger_t) lv_dropdown_get_selected(lv_event_get_target(e));
    params.dirty.charger = true;   /* saved by the params writer */
    radio_set_charger(params.charger);
}

static lv_obj_t *bt_switch_cell(uint8_t row, bool state, lv_event_cb_t cb) {
    lv_obj_t *obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 54);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = lv_switch_create(obj);
    styles_switch_init(obj);
    lv_obj_set_width(obj, SMALL_3 - 30);
    lv_obj_center(obj);

    /* Sin esto el switch no entra en el grupo de foco y no hay forma de cambiarlo con las
     * teclas ni con el MFK: era el caso de los dos del Bluetooth (docs/HALLAZGOS.md 18). */
    dialog_item(&dialog, obj);

    if (state) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    }

    lv_obj_add_event_cb(obj, cb, LV_EVENT_VALUE_CHANGED, NULL);

    return obj;
}

static uint8_t make_bluetooth(uint8_t row) {
    lv_obj_t *obj;

    /* Bluetooth on/off */
    row_dsc[row] = 54;
    obj = lv_label_create(grid);
    lv_label_set_text(obj, "Bluetooth");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    bool on = bt_state_on();

    bt_switch_cell(row, on, bt_power_update_cb);

    row++;

    /* Discoverable (for the CAT bridge) */
    row_dsc[row] = 54;
    obj = lv_label_create(grid);
    lv_label_set_text(obj, "Discoverable (CAT)");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    bt_visible_sw = bt_switch_cell(row, bt_state_discoverable(), bt_visible_update_cb);

    if (!on) {
        if (bt_visible_sw) lv_obj_add_state(bt_visible_sw, LV_STATE_DISABLED);
    }

    lv_obj_add_event_cb(bt_visible_sw, bt_visible_delete_cb, LV_EVENT_DELETE, NULL);

    row++;

    /* Devices: la UX para elegir a que dispositivo emparejarse (abre dialog_bt) */
    row_dsc[row] = 54;
    obj = lv_label_create(grid);
    lv_label_set_text(obj, "Devices");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_label_create(grid);
    lv_label_set_text(obj, " Pair / connect... ");
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    dialog_item(&dialog, obj);
    lv_obj_add_event_cb(obj, bt_devices_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(obj, bt_devices_cb, LV_EVENT_KEY, NULL);

    row++;

    /* Charger (their Settings keeps it next to Bluetooth) */
    row_dsc[row] = 54;
    obj = lv_label_create(grid);
    lv_label_set_text(obj, "Charger");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_dropdown_create(grid);
    lv_dropdown_set_options(obj, " Off \n On \n Shadow");
    /* Sin esto el desplegable dibuja su flecha por defecto (LV_SYMBOL_DOWN), que no esta en
     * ninguna fuente del equipo: salia como caja vacia. Estaba tapada porque el desplegable se
     * recortaba al pasar el borde del dialogo; al entrar en el esquema unico (#175) queda a la
     * vista, asi que se saca igual que en los otros desplegables de Ajustes. */
    lv_dropdown_set_symbol(obj, NULL);
    lv_dropdown_set_selected(obj, params.charger);
    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);
    dialog_item(&dialog, obj);      /* si no, no se puede elegir con las teclas */
    lv_obj_add_event_cb(obj, charger_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return row + 1;
}

/* The cluster column is a cfg item, not one of the params_* types */
static void cluster_column_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    bool on = lv_obj_has_state(obj, LV_STATE_CHECKED) != 0;

    subject_set_int(cfg.cluster_column.val, on ? 1 : 0);
    cluster_column_set_active(on);      /* also pauses or resumes its timer */
    cluster_column_refresh();
}

static uint8_t make_cluster_column(uint8_t row) {
    lv_obj_t *obj;
    uint8_t   col = 0;

    row_dsc[row] = 54;

    obj = lv_label_create(grid);
    lv_label_set_text(obj, "Cluster column");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);
    lv_obj_set_size(obj, SMALL_3, 54);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, COL_CONTROLES, SPAN_1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = lv_switch_create(obj);
    styles_switch_init(obj);
    lv_obj_set_width(obj, SMALL_3 - 30);
    lv_obj_center(obj);
    dialog_item(&dialog, obj);      /* si no, no se puede cambiar con las teclas */

    if (subject_get_int(cfg.cluster_column.val)) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    }

    lv_obj_add_event_cb(obj, cluster_column_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return row + 1;
}

static uint8_t make_delimiter(uint8_t row) {
    row_dsc[row] = 10;

    return row + 1;
}

static void construct_cb(lv_obj_t *parent) {
    /* Cuantos items entran al grupo de foco: es la medida de "todo Ajustes se puede navegar".
     * Un widget que no llama a dialog_item() no se puede ni alcanzar (ver docs/HALLAZGOS.md 18),
     * asi que este numero queda en el log para poder compararlo. */
    uint32_t items_antes = keyboard_group ? lv_group_get_obj_count(keyboard_group) : 0;
    if (mfk_inner != NULL) {
        mfk_inner->mode = ROT_MFK_INNER_INVERSE_MODE;
    }

    dialog.obj = dialog_init(parent);

    grid = lv_obj_create(dialog.obj);

    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    /* 100% del ancho del dialog y scroll SOLO vertical: no puede aparecer una barra horizontal. */
    lv_obj_set_size(grid, LV_PCT(100), 330);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scroll_dir(dialog.obj, LV_DIR_VER);
    lv_obj_set_style_text_color(grid, TC(text), 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(grid, SMALL_PAD, 0);
    lv_obj_set_style_pad_row(grid, 5, 0);
    /* La barra de scroll se dibuja encima del contenido: se le reserva el lugar a la derecha, si no
     * el ultimo campo de cada fila queda tapado (se leia "202" en vez de "2026", medido con una
     * captura del framebuffer el 13/10). */
    lv_obj_set_style_pad_right(grid, 14, 0);
    lv_obj_set_style_width(grid, 7, LV_PART_SCROLLBAR);

    lv_obj_center(grid);

    uint8_t row = 1;

    now = time(NULL);
    struct tm *t = localtime(&now);
    memcpy(&ts, t, sizeof(ts));

    /* Los grupos siguen el orden de los tabs (grp_primero/grp_ultimo guardan los indices del
     * grid con los que el tab salta a la seccion): por utilidad para hacer contactos (issue
     * #109). El primer grupo no lleva delimitador; los demas, si. */
    uint16_t n;

    /* 1. Operation */
    n = lv_obj_get_child_cnt(grid);
    row = make_freq_accel(row);
    row = make_bluetooth(row);
    row = make_cluster_column(row);
    row = make_wfview(row);

    for (uint8_t i = 0; i < TRANSVERTER_NUM; i++)
        row = make_transverter(row, i);
    grp_primero[GRP_OPERACION] = n;  grp_ultimo[GRP_OPERACION] = lv_obj_get_child_cnt(grid);

    /* 2. Audio */
    row = make_delimiter(row);
    n = lv_obj_get_child_cnt(grid);
    row = make_line_gain(row);
    row = make_audio_gain(row);
    row = make_audio_enh(row);
    row = make_sp_mode(row);
    row = make_voice(row);
    grp_primero[GRP_AUDIO] = n;      grp_ultimo[GRP_AUDIO] = lv_obj_get_child_cnt(grid);

    /* 3. Spectrum */
    row = make_delimiter(row);
    n = lv_obj_get_child_cnt(grid);
    row = make_auto(row);
    row = make_waterfall_line_zoom(row);
    row = make_waterfall_smooth_scroll(row);
    row = make_waterfall_battery(row);
    grp_primero[GRP_ESPECTRO] = n;   grp_ultimo[GRP_ESPECTRO] = lv_obj_get_child_cnt(grid);

    /* 4. Clock */
    row = make_delimiter(row);
    n = lv_obj_get_child_cnt(grid);
    row = make_date(row);
    row = make_time(row);
    row = make_clock(row);
    grp_primero[GRP_RELOJ] = n;      grp_ultimo[GRP_RELOJ] = lv_obj_get_child_cnt(grid);

    /* 5. Display */
    row = make_delimiter(row);
    n = lv_obj_get_child_cnt(grid);
    row = make_backlight(row);
    row = make_smeter(row);
    row = make_band_plan(row);
    grp_primero[GRP_PANTALLA] = n;   grp_ultimo[GRP_PANTALLA] = lv_obj_get_child_cnt(grid);

    /* 6. Appearance */
    row = make_delimiter(row);
    n = lv_obj_get_child_cnt(grid);
    row = make_theme(row);
    row = make_font(row);
    row = make_visual_profile(row);
    grp_primero[GRP_APARIENCIA] = n; grp_ultimo[GRP_APARIENCIA] = lv_obj_get_child_cnt(grid);

    /* 7. Actions */
    row = make_delimiter(row);
    n = lv_obj_get_child_cnt(grid);
    row = make_long_action(row);
    row = make_hmic_action(row);
    grp_primero[GRP_ACCIONES] = n;   grp_ultimo[GRP_ACCIONES] = lv_obj_get_child_cnt(grid);

    /* 8. Info */
    row = make_delimiter(row);
    n = lv_obj_get_child_cnt(grid);
    row = make_mag(row);
    grp_primero[GRP_INFO] = n;       grp_ultimo[GRP_INFO] = lv_obj_get_child_cnt(grid);

    row = make_delimiter(row);

    row_dsc[row] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    grp_actual = GRP_OPERACION;
    tabs_marks();

    for (uint8_t g = 0; g < GRP_COUNT; g++)
        LV_LOG_USER("Ajustes grupo %u: hijos %u..%u", g, grp_primero[g], grp_ultimo[g]);

    LV_LOG_USER("Ajustes: %u items navegables (creados en esta apertura)", 
                (unsigned) (keyboard_group ? lv_group_get_obj_count(keyboard_group) - items_antes : 0));
}

static void key_cb(lv_event_t * e) {
    uint32_t    key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case HKEY_FINP:
             lv_group_set_editing(keyboard_group, !lv_group_get_editing((const lv_group_t*) keyboard_group));
             break;

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

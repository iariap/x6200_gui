/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - posicion (la app GPS del X6100, adaptada).
 *
 *  El X6200 no tiene receptor GPS (no hay nodo en el device tree y el firmware base sacó gpsd),
 *  asi que la posicion sale de lo que mande el telefono o el configurador web por el socket de
 *  control remoto ({"cmd":"location","text":"45.0,7.7"}). La app lo dice tal cual y muestra la
 *  posicion, la grilla QTH y su antiguedad.
 */

#include "dialog_gps.h"

#include "events.h"

#include "fonts/font_select.h"
#include "gps.h"
#include "gps_format.h"
#include "keyboard.h"
#include "qth/qth.h"
#include "radio.h"
#include "styles.h"
#include "theme.h"

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define ROWS        7
#define HEIGHT      ((DIALOG_H - 12) / ROWS)
#define LABEL_WIDTH 340
#define VALUE_X     (30 + LABEL_WIDTH + 10)
#define VALUE_W     (796 - VALUE_X - 20)

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void key_cb(lv_event_t *e);

static dialog_t   dialog = {
    .run = false,
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .audio_cb = NULL,
    .key_cb = key_cb,
};
static lv_obj_t  *estado;
static lv_obj_t  *fijacion;
static lv_obj_t  *antiguedad;
static lv_obj_t  *fecha;
static lv_obj_t  *lat;
static lv_obj_t  *lon;
static lv_obj_t  *qth_lbl;
static lv_timer_t *timer;

static void actualizar(lv_timer_t *t);

static void construct_cb(lv_obj_t *parent) {
    lv_obj_t   *label;
    lv_coord_t  y = 6;

    dialog.obj = dialog_init(parent);
    lv_obj_set_style_text_font(dialog.obj, &sony_28, 0);

    lv_group_add_obj(keyboard_group, dialog.obj);

    struct {
        const char *texto;
        lv_obj_t  **destino;
    } filas[ROWS] = {
        {"Position source:", &estado},
        {"Fix:", &fijacion},
        {"Age:", &antiguedad},
        {"Date, time:", &fecha},
        {"Latitude:", &lat},
        {"Longitude:", &lon},
        {"QTH Grid:", &qth_lbl},
    };

    for (int i = 0; i < ROWS; i++) {
        label = lv_label_create(dialog.obj);
        lv_label_set_text(label, filas[i].texto);
        lv_obj_set_size(label, LABEL_WIDTH, HEIGHT);
        lv_obj_set_pos(label, 30, y);

        *filas[i].destino = lv_label_create(dialog.obj);
        lv_label_set_text(*filas[i].destino, "N/A");
        lv_obj_set_size(*filas[i].destino, VALUE_W, HEIGHT);
        lv_obj_set_pos(*filas[i].destino, VALUE_X, y);

        y += HEIGHT;
    }

    timer = lv_timer_create(actualizar, 500, NULL);
    lv_timer_ready(timer);
}

static void destruct_cb(void) {
    if (timer != NULL) {
        lv_timer_del(timer);
        timer = NULL;
    }
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *) lv_event_get_param(e));

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

/* Se refresca cada 500 ms: la posicion del telefono cambia sin avisar */
static void actualizar(lv_timer_t *t) {
    double   la = 0.0, lo = 0.0;
    int      age = -1;
    bool     hay = false;
    bool     del_telefono = false;
    lv_obj_t *dummy;

    (void) t;

    if (gps_last_fix(&la, &lo, &age)) {
        hay = true;
    } else if (gps_phone_fix(&la, &lo, &age)) {
        hay          = true;
        del_telefono = true;
    }

    lv_label_set_text(estado, hay && del_telefono ? "Phone / web" : "None: this radio has no GPS");

    if (!hay) {
        lv_label_set_text(fijacion, "None");
        lv_label_set_text(antiguedad, "No data");
        lv_label_set_text(lat, "N/A");
        lv_label_set_text(lon, "N/A");
        lv_label_set_text(qth_lbl, "N/A");
    } else {
        char buf[64];

        lv_label_set_text(fijacion, del_telefono ? "Phone" : "Internal");
        gps_format_age(age, buf, sizeof(buf));
        lv_label_set_text(antiguedad, buf);

        gps_format_coord(la, "N", "S", buf, sizeof(buf));
        lv_label_set_text(lat, buf);

        gps_format_coord(lo, "E", "W", buf, sizeof(buf));
        lv_label_set_text(lon, buf);

        char grid[9];

        qth_pos_to_str(la, lo, grid);
        lv_label_set_text(qth_lbl, grid);
    }

    /* La hora del equipo (la pone el NTP, no hay RTC) */
    time_t    ahora = time(NULL);
    struct tm tm;

    localtime_r(&ahora, &tm);

    char fecha_txt[32];

    if (strftime(fecha_txt, sizeof(fecha_txt), "%Y-%m-%d %H:%M:%S", &tm) > 0) {
        lv_label_set_text(fecha, fecha_txt);
    } else {
        lv_label_set_text(fecha, "N/A");
    }

    dummy = NULL;   /* (sin uso: deja claro que no hay mas widgets) */
    (void) dummy;
}

dialog_t *dialog_gps = &dialog;

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Bluetooth devices (Settings -> Bluetooth).
 *
 *  The pairing itself is done by btaudio-ctl: this app shows what it reports,
 *  runs scan/pair/connect/disconnect for you and shows the passkey a keyboard
 *  wants typed on it. The script is run off the UI thread and its output lands in
 *  /tmp: a timer reads it back, parses it with bt_devices_parse() and redraws.
 */

#include "dialog_bt.h"

#include "btaudio.h"
#include "bt_devices.h"
#include "buttons.h"
#include "fonts/font_select.h"
#include "keyboard.h"
#include "dialog.h"
#include "msg.h"
#include "styles.h"
#include "theme.h"
#include "util.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BT_CTL          "/usr/sbin/btaudio-ctl"
#define BT_LIST_FILE    "/tmp/bt_ui_list.txt"
#define BT_SCAN_FILE    "/tmp/bt_ui_scan.txt"
#define BT_PASSKEY      "/tmp/bt_passkey"

#define DIALOG_W        771
#define HEADER_H        40
#define STATUS_H        30
#define TABLE_Y         (10 + HEADER_H)
#define TABLE_H         (DIALOG_BOX_H - TABLE_Y - STATUS_H - 8)
#define REFRESH_MS      700

/* The dialog's font: one step down in the VISION theme, like the other dialogs */
#define VF(n) (theme_shape.vision ? font_down(&sony_##n, VISION_FONT_STEPS - 1) : &sony_##n)

static lv_obj_t   *table;
static lv_obj_t   *label_state;
static lv_obj_t   *label_note;
static lv_timer_t *timer;

static bt_list_t  shown;            /* what the table is showing */
static char       last_text[4096];  /* to redraw only when it changed */

/* Hasta cuando corre el scan (get_time()), para no lanzar dos a la vez y para avisar del avance:
 * cada pulsacion del boton arrancaba otro scan de 15 s y los dos escribian el mismo archivo, asi
 * que el usuario, que apretaba varias veces, veia menos resultados que sin apretar (medido el
 * 13/10: /tmp/bt_ui_scan.txt y "btaudio-ctl scan" a mano dan el mismo dispositivo). */
#define SCAN_MS     15000

static uint64_t   scan_until;

static void refresh_cb(lv_timer_t *t);

/* Los botones (definidos mas abajo): refresh_cb los refresca. */
static button_data_t btn_scan;
static button_data_t btn_auto;

/* Etiqueta del boton Scan: es el affordance de que el scan esta corriendo. El boton queda
 * resaltado (buttons_mark), bloqueado (buttons_disabled: buttons_press lo ignora) y con la cuenta
 * regresiva, asi el usuario ve la operacion en curso sin mirar la linea de estado. */
static const char *scan_label(void) {
    static char buf[24];

    if (scan_until != 0 && get_time() < scan_until) {
        snprintf(buf, sizeof(buf), "Scan %llus", (unsigned long long) ((scan_until - get_time()) / 1000));
        return buf;
    }

    return "Scan";
}

/* --- running the script -------------------------------------------------- */

/* Runs "<a command>; btaudio-ctl list > BT_LIST_FILE" so the file always ends up
 * holding a fresh list: the commands that change something print nothing. */
static void *run_thread(void *arg) {
    char *cmd = arg;

    int rc = system(cmd);

    (void) rc;
    free(cmd);

    return NULL;
}

static void bt_run(const char *cmd, bool scanning) {
    size_t size = strlen(cmd) + 256;
    char  *line = malloc(size);

    if (!line) {
        return;
    }

    snprintf(line, size,
             "%s %s >/dev/null 2>&1; %s list > " BT_LIST_FILE " 2>&1",
             BT_CTL, cmd, BT_CTL);

    if (scanning) {
        /* The scan takes about 15 s and its results are the devices that are not
         * paired yet: keep them in their own file so the list is not lost. */
        snprintf(line, size,
                 "%s scan > " BT_SCAN_FILE " 2>&1; %s list > " BT_LIST_FILE " 2>&1",
                 BT_CTL, BT_CTL);
    }

    pthread_t t;

    btaudio_log("ui: btaudio-ctl %s", scanning ? "scan (+list)" : cmd);

    if (pthread_create(&t, NULL, run_thread, line) == 0) {
        pthread_detach(t);
    } else {
        free(line);
    }
}

/* --- reading what the script said ---------------------------------------- */

static bool read_file(const char *path, char *buf, size_t size) {
    FILE *f = fopen(path, "r");

    if (!f) {
        return false;
    }

    size_t n = fread(buf, 1, size - 1, f);

    fclose(f);
    buf[n] = '\0';

    return true;
}

/* The list plus the devices the last scan found (the ones not paired yet) */
static void bt_read_state(bt_list_t *out) {
    char text[4096];

    text[0] = '\0';
    read_file(BT_LIST_FILE, text, sizeof(text));
    bt_devices_parse(text, out);

    char scan[2048];

    if (read_file(BT_SCAN_FILE, scan, sizeof(scan))) {
        bt_list_t new_devs;

        bt_devices_parse(scan, &new_devs);

        for (int i = 0; i < new_devs.n && out->n < BT_MAX_DEV; i++) {
            /* Not already in the list */
            bool dup = false;

            for (int j = 0; j < out->n; j++) {
                if (!strcmp(out->dev[j].mac, new_devs.dev[i].mac)) {
                    dup = true;
                }
            }

            if (!dup) {
                out->dev[out->n++] = new_devs.dev[i];
            }
        }
    }
}

static const char *role_name(const char *role) {
    if (!strcmp(role, "hp")) {
        return "headphones";
    }

    if (!strcmp(role, "kbd")) {
        return "keyboard";
    }

    if (!strcmp(role, "cat")) {
        return "CAT";
    }

    return role;
}

static void fill_table(bt_list_t *l) {
    lv_table_set_row_cnt(table, l->n > 0 ? l->n : 1);
    lv_table_set_col_cnt(table, 3);
    lv_table_set_col_width(table, 0, 430);
    lv_table_set_col_width(table, 1, 150);
    lv_table_set_col_width(table, 2, DIALOG_W - 430 - 150 - 20);

    for (int i = 0; i < l->n; i++) {
        const bt_device_t *d = &l->dev[i];

        lv_table_set_cell_value(table, i, 0, d->name[0] ? d->name : d->mac);
        lv_table_set_cell_value(table, i, 1, role_name(d->role));
        lv_table_set_cell_value(table, i, 2, bt_device_state(d));
    }

    if (l->n == 0) {
        lv_table_set_cell_value(table, 0, 0, "No devices: press Scan");
        lv_table_set_cell_value(table, 0, 1, "");
        lv_table_set_cell_value(table, 0, 2, "");
    }

    char state[128];

    if (l->hp && l->hp_volume >= 0) {
        snprintf(state, sizeof(state), "headphones %d%%", l->hp_volume);
    } else {
        snprintf(state, sizeof(state), "%s", l->hp ? "headphones present" : "no headphones");
    }

    strncat(state, l->rx ? ", RX on" : ", RX off", sizeof(state) - strlen(state) - 1);

    if (l->mute) {
        strncat(state, ", speaker muted", sizeof(state) - strlen(state) - 1);
    }

    lv_label_set_text(label_state, state);
}

static const bt_device_t *selected_device(void) {
    uint16_t row;
    uint16_t col;

    /* El marco de botones pide la etiqueta de "Auto" al armar la pagina, y eso pasa antes de que
     * exista la tabla: sin este chequeo, abrir la app se llevaba la GUI puesta (crash medido el
     * 13/10: el home volvia solo y no habia log). */
    if (table == NULL) {
        return NULL;
    }

    lv_table_get_selected_cell(table, &row, &col);

    if (row >= (uint16_t) shown.n) {
        return NULL;
    }

    return &shown.dev[row];
}

/* Auto-connect por dispositivo: el script lo guarda por MAC (BTAUDIO_AUTOCONNECT) y btaudio-ctl
 * boot lo usa para reconectar solo al arrancar. En el boton: pulsacion = prender/apagar, y el
 * hold = desemparejar (remove), que es lo destructivo y por eso va detras del long press. */
static const char *auto_label(void) {
    const bt_device_t *d = selected_device();

    if (d == NULL || !d->paired) {
        return "Auto:\n--";
    }

    return d->autoconnect ? "Auto:\nOn" : "Auto:\nOff";
}

static void auto_cb(button_data_t *data) {
    const bt_device_t *d = selected_device();

    if (d == NULL || !d->paired) {
        lv_label_set_text(label_note, "Pick a paired device first");
        buttons_refresh(data);
        return;
    }

    bool on = !d->autoconnect;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "autoconnect %s %s", d->mac, on ? "on" : "off");
    bt_run(cmd, false);

    lv_label_set_text_fmt(label_note, "Auto-connect %s for %s", on ? "on" : "off", d->name);
    buttons_refresh(data);
}

static void unpair_cb(button_data_t *data) {
    const bt_device_t *d = selected_device();

    if (d == NULL || !d->paired) {
        lv_label_set_text(label_note, "Pick a paired device first");
        buttons_refresh(data);
        return;
    }

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "remove %s", d->mac);
    bt_run(cmd, false);

    lv_label_set_text_fmt(label_note, "Unpairing %s...", d->name);
    buttons_refresh(data);
}

static void refresh_cb(lv_timer_t *t) {
    (void) t;

    char text[4096];

    text[0] = '\0';
    read_file(BT_LIST_FILE, text, sizeof(text));

    char scan[2048];

    if (read_file(BT_SCAN_FILE, scan, sizeof(scan))) {
        strncat(text, scan, sizeof(text) - strlen(text) - 1);
    }

    if (strcmp(text, last_text) != 0) {
        snprintf(last_text, sizeof(last_text), "%s", text);
        bt_read_state(&shown);
        fill_table(&shown);
        buttons_refresh(&btn_auto);
    }

    /* El scan es lo unico que el usuario no puede ver: mientras corre se muestra el tiempo que
     * falta y al terminar cuantos dispositivos encontro. Antes quedaba el cartel "Scanning (15
     * s)..." puesto y parecia que el boton no hacia nada. */
    if (scan_until != 0) {
        uint64_t now = get_time();

        if (now < scan_until) {
            buttons_refresh(&btn_scan);     /* el boton lleva la cuenta regresiva */
        } else {
            scan_until = 0;

            buttons_mark(&btn_scan, false);
            buttons_disabled(&btn_scan, false);
            buttons_refresh(&btn_scan);     /* vuelve a decir "Scan" */

            char scan[2048];
            int  n = 0;

            if (read_file(BT_SCAN_FILE, scan, sizeof(scan))) {
                for (char *p = scan; *p != '\0'; p++) {
                    if (*p == '\n' && p[1] != '\0' && p[1] != '\n') {
                        n++;
                    }
                }

                if (scan[0] != '\0' && scan[strlen(scan) - 1] != '\n') {
                    n++;
                }
            }

            if (n > 0) {
                lv_label_set_text_fmt(label_note, "Scan done: %i device(s)", n);
            } else {
                lv_label_set_text(label_note, "Scan done: nothing found");
            }
        }
    }

    /* While pairing, a keyboard asks for this code to be typed on it */
    char code[16];

    if (read_file(BT_PASSKEY, code, sizeof(code)) && code[0]) {
        code[strcspn(code, "\r\n")] = '\0';
        lv_label_set_text_fmt(label_note, "Type %s and Enter on the keyboard", code);
    }
}

/* --- actions ------------------------------------------------------------- */

static void scan_cb(button_data_t *data) {
    if (scan_until != 0 && get_time() < scan_until) {
        /* Ya hay uno corriendo: no lanzar otro (se pisarian los resultados) */
        lv_label_set_text(label_note, "Scan already running...");
        buttons_refresh(data);
        return;
    }

    scan_until = get_time() + SCAN_MS + 1000;   /* el scan del script tarda 15 s */
    bt_run("scan", true);

    buttons_mark(data, true);           /* resaltado: se ve que esta corriendo */
    buttons_disabled(data, true);       /* bloqueado: no se puede lanzar otro */
    lv_label_set_text(label_note, "Scanning...");
    buttons_refresh(data);
}

static void row_cb(lv_event_t *e) {
    uint16_t row;
    uint16_t col;

    lv_table_get_selected_cell(table, &row, &col);

    if (row >= (uint16_t) shown.n) {
        return;
    }

    const bt_device_t *d = &shown.dev[row];
    char               cmd[128];

    if (!d->paired) {
        snprintf(cmd, sizeof(cmd), "pair %s", d->mac);
        lv_label_set_text_fmt(label_note, "Pairing %s...", d->mac);
    } else if (!d->connected) {
        snprintf(cmd, sizeof(cmd), "connect %s", d->mac);
        lv_label_set_text_fmt(label_note, "Connecting %s...", d->mac);
    } else {
        snprintf(cmd, sizeof(cmd), "disconnect %s", d->mac);
        lv_label_set_text_fmt(label_note, "Disconnecting %s...", d->mac);
    }

    bt_run(cmd, false);
}

static void rx_cb(button_data_t *data) {
    bt_run(shown.rx ? "rx off" : "rx on", false);
    lv_label_set_text_fmt(label_note, "RX to headphones: %s", shown.rx ? "off" : "on");
    buttons_refresh(data);
}

static void mute_cb(button_data_t *data) {
    bool mute = !shown.mute;

    bt_run(mute ? "mute on" : "mute off", false);
    btaudio_speaker_mute(mute);         /* el flag por si solo no apagaba nada */
    lv_label_set_text_fmt(label_note, "Speaker muted: %s", mute ? "on" : "off");
    buttons_refresh(data);
}

static const char *rx_label(void) {
    return shown.rx ? "RX to HP:\nOn" : "RX to HP:\nOff";
}

static const char *mute_label(void) {
    return shown.mute ? "Speaker:\nMuted" : "Speaker:\nOn";
}

static void close_cb(button_data_t *data) {
    (void) data;

    dialog_destruct();
}

static button_data_t btn_scan = { .type = BTN_TEXT_FN, .label_fn = scan_label, .press = scan_cb };
static button_data_t btn_rx   = { .type = BTN_TEXT_FN, .label_fn = rx_label, .press = rx_cb };
static button_data_t btn_mute = { .type = BTN_TEXT_FN, .label_fn = mute_label, .press = mute_cb };
static button_data_t btn_auto  = { .type = BTN_TEXT_FN, .label_fn = auto_label, .press = auto_cb, .hold = unpair_cb };
static button_data_t btn_close = { .type = BTN_TEXT, .label = "Close", .press = close_cb };

static buttons_page_t btn_page = {
    .items = {&btn_scan, &btn_rx, &btn_mute, &btn_auto, &btn_close},
};

/* --- dialog -------------------------------------------------------------- */

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void key_cb(lv_event_t *e);

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .key_cb       = key_cb,
    .btn_page     = &btn_page,
};

dialog_t *dialog_bt = &dialog;

static lv_obj_t *make_label(lv_obj_t *parent, lv_color_t color) {
    lv_obj_t *l = lv_label_create(parent);

    lv_obj_set_style_text_font(l, VF(22), 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_recolor(l, true);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);

    return l;
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    lv_obj_t *head = make_label(dialog.obj, TC(text_head));

    lv_label_set_text(head, "Bluetooth devices");
    lv_obj_set_pos(head, 18, 12);

    table = lv_table_create(dialog.obj);
    lv_obj_remove_style(table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_set_size(table, DIALOG_W, TABLE_H);
    lv_obj_set_pos(table, 13, TABLE_Y);
    lv_obj_set_style_bg_opa(table, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(table, TC(panel), LV_PART_MAIN);
    lv_obj_set_style_border_width(table, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(table, TC(dialog_border), LV_PART_MAIN);
    lv_obj_set_style_border_opa(table, 96, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(table, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(table, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(table, VF(22), LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, TC(text), LV_PART_ITEMS);
    lv_obj_set_style_pad_top(table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(table, 5, LV_PART_ITEMS);
    lv_obj_add_event_cb(table, row_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(keyboard_group, table);

    label_state = make_label(dialog.obj, TC(text_dim));
    lv_obj_set_pos(label_state, 18, DIALOG_BOX_H - STATUS_H - 4);
    lv_obj_set_width(label_state, 380);
    lv_label_set_text(label_state, "");

    label_note = make_label(dialog.obj, TC(text));
    lv_obj_set_width(label_note, 360);
    lv_obj_set_style_text_align(label_note, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label_note, DIALOG_W + 13 - 18 - 360, DIALOG_BOX_H - STATUS_H - 4);
    lv_label_set_text(label_note, "Row: pair/connect - Auto: hold to unpair");

    shown.n  = 0;
    last_text[0] = '\0';

    /* Ask for the list right away */
    bt_run("list", false);

    timer = lv_timer_create(refresh_cb, REFRESH_MS, NULL);
}

static void destruct_cb(void) {
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }

    table = NULL;
}

static void key_cb(lv_event_t *e) {
    uint32_t key = lv_event_get_key(e);

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct();
            break;

        case LV_KEY_ENTER:
        case LV_KEY_RIGHT:
            row_cb(e);
            break;
    }
}

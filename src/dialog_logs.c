/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - App "Log uploads".
 *
 *  Edita las credenciales de la subida automatica de QSO a los logs online. El
 *  archivo es /mnt/upload.conf y el daemon lo relee en cada pasada (30 s), asi que
 *  los cambios aplican solos, sin reiniciar nada.
 *
 *  La pantalla usa la MISMA grilla de Ajustes (col_dsc, filas de 54 px, delimitadores,
 *  switch de styles_switch_init) para que se vea y se maneje igual: se navega con la
 *  perilla/teclas y se toca la fila para editar.
 */
#include "dialog_logs.h"

#include "dialog.h"
#include "events.h"
#include "fonts/font_select.h"
#include "keyboard.h"
#include "msg.h"
#include "styles.h"
#include "textarea_window.h"
#include "theme.h"
#include "upload_conf.h"

#include <stdio.h>
#include <string.h>

/* --- Misma grilla que dialog_settings.c (issue #175) --------------------- */
#define SMALL_PAD   3
#define SMALL_1     45
#define SMALL_3     (SMALL_1 * 3 + SMALL_PAD * 2)
#define SMALL_6     (SMALL_1 * 6 + SMALL_PAD * 5)
#define COL_CONTROLES 1
#define ROW_LABEL_PAD 6

static lv_coord_t col_dsc[] = { LV_GRID_FR(1), SMALL_1, SMALL_1, SMALL_1, SMALL_1, SMALL_1, SMALL_1, LV_GRID_TEMPLATE_LAST };
static lv_coord_t row_dsc[48] = { 1 };

#define ROWS_MAX  12

typedef struct {
    const char *label;
    bool        secret;
    char       *value;
    size_t      size;
    lv_obj_t   *field;
} row_desc_t;

static dialog_t      dialog;
static upload_conf_t conf;
static lv_obj_t     *grid = NULL;

static row_desc_t    rows[ROWS_MAX];
static int           n_rows;

static char       *edit_field = NULL;
static size_t      edit_size = 0;
static row_desc_t *edit_row = NULL;

/* Igual que en Ajustes: la etiqueta trae su propia superficie del tema para que la
 * tinta se lea en los seis temas (ver el comentario de row_label en dialog_settings.c). */
static lv_obj_t *row_label(lv_obj_t *parent, const char *text) {
    lv_obj_t *l = lv_label_create(parent);

    lv_obj_set_style_bg_color(l, TC(bg), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(l, ROW_LABEL_PAD, 0);
    lv_obj_set_style_translate_x(l, -ROW_LABEL_PAD, 0);
    lv_label_set_text(l, text);

    return l;
}

/* Celda de la zona de controles: misma caja que usan los switches de Ajustes. */
static lv_obj_t *controls_cell(int row, lv_coord_t width) {
    lv_obj_t *cell = lv_obj_create(grid);

    lv_obj_set_size(cell, width, 54);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, COL_CONTROLES, 6, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    return cell;
}

static void switch_cb(lv_event_t *e) {
    bool *flag = lv_event_get_user_data(e);

    if (flag) {
        *flag = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        upload_conf_save(&conf);
        msg_update_text_fmt("Log upload: saved");
    }
}

static void field_refresh(row_desc_t *r) {
    if (r->field == NULL) {
        return;
    }

    if (r->secret) {
        lv_textarea_set_text(r->field, r->value[0] ? "********" : "(tap to set)");
    } else {
        lv_textarea_set_text(r->field, r->value[0] ? r->value : "(tap to set)");
    }
}

static bool edit_ok(void) {
    const char *text = textarea_window_get();

    if (edit_field != NULL) {
        snprintf(edit_field, edit_size, "%s", text ? text : "");
        upload_conf_save(&conf);
        field_refresh(edit_row);
        msg_update_text_fmt("Log upload: saved");
    }

    edit_field = NULL;
    edit_row   = NULL;

    return true;
}

static bool edit_cancel(void) {
    edit_field = NULL;
    edit_row   = NULL;

    return true;
}

static void field_click_cb(lv_event_t *e) {
    row_desc_t *r = lv_event_get_user_data(e);

    if (r == NULL || r->value == NULL) {
        return;
    }

    edit_field = r->value;
    edit_size  = r->size;
    edit_row   = r;

    textarea_window_open_w_label(edit_ok, edit_cancel, r->label);
    textarea_window_set(r->value);
}

static uint8_t make_switch_row(uint8_t row, const char *label, bool *flag) {
    lv_obj_t *obj;

    row_dsc[row] = 54;

    obj = row_label(grid, label);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = controls_cell(row, SMALL_3);
    obj = lv_switch_create(obj);

    styles_switch_init(obj);
    lv_obj_set_width(obj, SMALL_3 - 30);
    lv_obj_center(obj);
    dialog_item(&dialog, obj);
    lv_obj_add_event_cb(obj, switch_cb, LV_EVENT_VALUE_CHANGED, flag);

    if (*flag) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    }

    return row + 1;
}

static uint8_t make_field_row(uint8_t row, const char *label, bool secret, char *value, size_t size) {
    lv_obj_t *obj;

    row_dsc[row] = 54;

    obj = row_label(grid, label);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    row_desc_t *r = &rows[n_rows++];

    r->label  = label;
    r->secret = secret;
    r->value  = value;
    r->size   = size;

    lv_obj_t *cell = controls_cell(row, SMALL_6);

    r->field = lv_textarea_create(cell);
    lv_obj_set_size(r->field, SMALL_6 - 10, 44);
    lv_obj_center(r->field);
    lv_textarea_set_one_line(r->field, true);
    lv_textarea_set_max_length(r->field, size - 1);
    lv_obj_clear_flag(r->field, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_align(r->field, LV_TEXT_ALIGN_RIGHT, 0);

    dialog_item(&dialog, r->field);
    lv_obj_add_event_cb(r->field, field_click_cb, LV_EVENT_CLICKED, r);

    field_refresh(r);

    return row + 1;
}

static uint8_t make_delimiter(uint8_t row) {
    row_dsc[row] = 10;

    return row + 1;
}

static void close_cb(button_data_t *b) {
    (void) b;
    dialog_destruct();
}

static button_data_t btn_close = { .type = BTN_TEXT, .label = "Close", .press = close_cb };

static buttons_page_t btn_page = {
    .items = {NULL, NULL, NULL, NULL, &btn_close},
};

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *) lv_event_get_param(e));

    if (key == LV_KEY_ESC) {
        dialog_destruct();
    }
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);
    n_rows     = 0;

    upload_conf_load(&conf);

    lv_obj_t *title = lv_label_create(dialog.obj);

    lv_obj_set_style_text_font(title, &sony_28, 0);
    lv_obj_set_style_text_color(title, TC(text_head), 0);
    lv_label_set_text(title, "Log uploads");
    lv_obj_set_pos(title, 24, 4);

    grid = lv_obj_create(dialog.obj);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    lv_obj_set_size(grid, LV_PCT(100), 292);
    lv_obj_set_pos(grid, 0, 44);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scroll_dir(dialog.obj, LV_DIR_VER);
    lv_obj_set_style_text_color(grid, TC(text), 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(grid, SMALL_PAD, 0);
    lv_obj_set_style_pad_row(grid, 5, 0);
    lv_obj_set_style_pad_right(grid, 14, 0);
    lv_obj_set_style_width(grid, 7, LV_PART_SCROLLBAR);

    uint8_t row = 1;

    row = make_switch_row(row, "Upload", &conf.enabled);

    row = make_delimiter(row);
    row = make_switch_row(row, "eQSL", &conf.eqsl_enabled);
    row = make_field_row(row, "eQSL user", false, conf.eqsl_user, sizeof(conf.eqsl_user));
    row = make_field_row(row, "eQSL password", true, conf.eqsl_pswd, sizeof(conf.eqsl_pswd));

    row = make_delimiter(row);
    row = make_switch_row(row, "QRZ", &conf.qrz_enabled);
    row = make_field_row(row, "QRZ API key", true, conf.qrz_api_key, sizeof(conf.qrz_api_key));

    row = make_delimiter(row);
    row = make_switch_row(row, "Clublog", &conf.clublog_enabled);
    row = make_field_row(row, "Clublog callsign", false, conf.clublog_callsign, sizeof(conf.clublog_callsign));
    row = make_field_row(row, "Clublog email", false, conf.clublog_email, sizeof(conf.clublog_email));
    row = make_field_row(row, "Clublog password", true, conf.clublog_password, sizeof(conf.clublog_password));
    row = make_field_row(row, "Clublog API key", true, conf.clublog_api, sizeof(conf.clublog_api));

    row_dsc[row] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
}

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = NULL,
    .key_cb       = key_cb,
    .btn_page     = &btn_page,
};

dialog_t *dialog_logs = &dialog;

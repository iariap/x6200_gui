/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - colour roles (see theme.h).
 *
 *  Default colours of the X6100's built-in theme, which is what the ported app
 *  dialogs were designed against. They do not change this GUI's own screens:
 *  only the ported app code asks for them.
 */

#include "theme.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stddef.h>

typedef struct {
    theme_color_t role;
    uint32_t      hex;
} theme_entry_t;

static const theme_entry_t colors[] = {
    { TC_bg,                 0x1c1f24 },
    { TC_bar,                0x1c1f24 },
    { TC_bar_grad,           0x1c1f24 },
    { TC_bar_border,         0x5dcaa5 },
    { TC_strip,              0x262a31 },
    { TC_strip_grad,         0x262a31 },
    { TC_strip_border,       0x5dcaa5 },
    { TC_dialog,             0x262a31 },
    { TC_dialog_grad,        0x262a31 },
    { TC_dialog_border,      0x5dcaa5 },
    { TC_btn,                0x2a3038 },
    { TC_btn_grad,           0x2a3038 },
    { TC_btn_border,         0x5dcaa5 },
    { TC_btn_text,           0xffffff },
    { TC_btn_active,         0xff8a30 },
    { TC_btn_active_grad,    0xff8a30 },
    { TC_btn_active_border,  0xff8a30 },
    { TC_btn_active_text,    0x2a1200 },
    { TC_btn_page,           0x2a3038 },
    { TC_btn_page_text,      0xe6fff6 },
    { TC_text,               0xffffff },
    { TC_text_value,         0xffffff },
    { TC_text_soft,          0xe6fff6 },
    { TC_text_head,          0x5dcaa5 },
    { TC_text_dim,           0x80868e },
    { TC_text_mute,          0x5a5f66 },
    { TC_mode_text,          0x2a1200 },
    { TC_mode_bg,            0xff8a30 },
    { TC_mode_border,        0xff8a30 },
    { TC_pill_border,        0x5dcaa5 },
    { TC_pill_off_text,      0x80868e },
    { TC_focus,              0xff8a30 },
    { TC_focus_text,         0x2a1200 },
    { TC_meter_bar,          0x5dcaa5 },
    { TC_meter_weak,         0x5dcaa5 },
    { TC_meter_low,          0x5dcaa5 },
    { TC_meter_high,         0xff5a3c },
    { TC_meter_over,         0xffd040 },
    { TC_popup,              0x262a31 },
    { TC_popup_border,       0x5dcaa5 },
    { TC_panel,              0x1c1f24 },
    { TC_frame,              0x5dcaa5 },
    { TC_track,              0x2a3038 },
    { TC_accent,             0x5dcaa5 },
    { TC_tx,                 0xff5a3c },
    { TC_text_faint,         0x5a5f66 },
    { TC_err,                0xff5a3c },
    { TC_near_bg,            0x262a31 },
    { TC_warn,               0xffd040 },
    { TC_alert,              0xff5a3c },
    { TC_alert_bg,           0x2a1200 },
    { TC_alert_text,         0xffffff },
    { TC_alert_box,          0x5dcaa5 },
    { TC_vfo,                0x5dcaa5 },
    { TC_vfo_bg,             0x1c1f24 },
    { TC_new,                0xff8a30 },
    { TC_veil,               0x1c1f24 },
    { TC_info,               0x5dcaa5 },
    { TC_chip,               0x2a3038 },
    { TC_btn_mark,           0xff8a30 },
    { TC_panel_border,       0x5dcaa5 },
    { TC_accent_soft,        0x8fd8c0 },
    { TC_action,             0xff8a30 },
    { TC_focus_bg,           0xff8a30 },
    { TC_list,               0x1c1f24 },
    { TC_title_text,         0x5dcaa5 },
    { TC_tx_bg,              0x3a1414 },
    { TC_warn_bg,            0x3a2f10 },
    { TC_row,                0xffffff },
    { TC_row_alert,          0xff8a30 },
    { TC_row_heard,          0xffffff },
    { TC_row_tx,             0xff5a3c },
    { TC_row_to_me,          0x5dcaa5 },
    { TC_row_cq,             0xff8a30 },
    { TC_row_group,          0x5aa9ff },
    { TC_row_sel_bar,        0x5dcaa5 },
    { TC_row_alert_text,     0xff5a3c },
    { TC_ok,                 0x5dcaa5 },
    { TC_ok_bar,             0x5dcaa5 },
    { TC_sel_bg,             0x5dcaa5 },
    { TC_sel_border,         0x5dcaa5 },
    { TC_sel_text,           0x2a1200 },
};

/* Flat, like the app dialogs: no gradients, square corners, no VISION layout */
theme_shape_t theme_shape = {
    .name            = "x6200",
    .radius          = 0,
    .radius_btn      = 0,
    .radius_popup    = 0,
    .border          = 1,
    .btn_gradient    = false,
    .box_gradient    = false,
    .focus_solid     = true,
    .meter_gradient  = false,
    .mono_cluster    = false,
    .images          = 0,
    .font            = -1,
    .mark            = { 0, 0, 0 },
    .pill_border     = 1,
    .mode_box        = false,
    .btn_gap         = 0,
    .rule            = false,
    .vision          = false,
};

uint32_t theme_hex(theme_color_t c) {
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        if (colors[i].role == c) {
            return colors[i].hex;
        }
    }

    return 0xffffff;    /* unknown role: readable by default */
}

/* Icons: the ported apps ask for PNG files by path and keep the descriptor.
 * Loaded once (a handful of Field Ops icons), kept for the process. */
#define THEME_PNG_MAX 16

static struct {
    char             path[128];
    lv_img_dsc_t     dsc;
    uint8_t         *data;
} pngs[THEME_PNG_MAX];
static int pngs_n;

const lv_img_dsc_t *theme_png(const char *path) {
    if (!path || !*path) {
        return NULL;
    }

    for (int i = 0; i < pngs_n; i++) {
        if (strcmp(pngs[i].path, path) == 0) {
            return &pngs[i].dsc;
        }
    }

    if (pngs_n == THEME_PNG_MAX) {
        return NULL;
    }

    FILE *f = fopen(path, "rb");

    if (!f) {
        return NULL;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   info = png ? png_create_info_struct(png) : NULL;

    if (!png || !info || setjmp(png_jmpbuf(png))) {
        if (png) {
            png_destroy_read_struct(&png, info ? &info : NULL, NULL);
        }

        fclose(f);

        return NULL;
    }

    png_init_io(png, f);
    png_read_info(png, info);

    uint32_t w = png_get_image_width(png, info);
    uint32_t h = png_get_image_height(png, info);
    int      type = png_get_color_type(png, info);
    int      depth = png_get_bit_depth(png, info);

    if (depth == 16) png_set_strip_16(png);
    if (type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (type == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (type == PNG_COLOR_TYPE_RGB || type == PNG_COLOR_TYPE_GRAY) png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (type == PNG_COLOR_TYPE_GRAY || type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    size_t   row = png_get_rowbytes(png, info);
    uint8_t *data = malloc(row * h);
    png_bytep rows[512];

    if (!data || h > 512) {
        free(data);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(f);

        return NULL;
    }

    for (uint32_t y = 0; y < h; y++) {
        rows[y] = data + (size_t) y * row;
    }

    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(f);

    int slot = pngs_n++;

    snprintf(pngs[slot].path, sizeof(pngs[slot].path), "%s", path);
    pngs[slot].data = data;
    pngs[slot].dsc.header.cf   = LV_IMG_CF_TRUE_COLOR_ALPHA;
    pngs[slot].dsc.header.w    = w;
    pngs[slot].dsc.header.h    = h;
    pngs[slot].dsc.data_size   = (uint32_t) (row * h);
    pngs[slot].dsc.data        = data;

    return &pngs[slot].dsc;
}

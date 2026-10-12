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

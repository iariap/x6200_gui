/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - colour roles for the ported app code.
 *
 *  The X6100 paints its app dialogs through a JSON theme: code never writes a
 *  colour, it asks for a role (TC(text), theme_hex(TC_text)). Porting that code
 *  means giving it the same API backed by this GUI's fixed palette. When the
 *  JSON themes themselves are ported, theme.c becomes a loader and the callers
 *  stay unchanged.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TC_bg = 0,
    TC_bar, TC_bar_grad, TC_bar_border,
    TC_strip, TC_strip_grad, TC_strip_border,
    TC_dialog, TC_dialog_grad, TC_dialog_border,
    TC_btn, TC_btn_grad, TC_btn_border, TC_btn_text,
    TC_btn_active, TC_btn_active_grad, TC_btn_active_border, TC_btn_active_text,
    TC_btn_page, TC_btn_page_text,
    TC_text, TC_text_value, TC_text_soft, TC_text_head, TC_text_dim, TC_text_mute,
    TC_mode_text, TC_mode_bg, TC_mode_border,
    TC_pill_border, TC_pill_off_text,
    TC_focus, TC_focus_text,
    TC_meter_bar, TC_meter_weak, TC_meter_low, TC_meter_high, TC_meter_over,
    TC_popup, TC_popup_border,
    TC_panel, TC_frame,
    TC_track, TC_ok, TC_ok_bar,
    TC_accent, TC_tx, TC_text_faint, TC_err, TC_near_bg, TC_warn,
    TC_alert, TC_alert_bg, TC_alert_text, TC_alert_box, TC_vfo, TC_vfo_bg, TC_new, TC_veil,
    TC_info, TC_chip, TC_btn_mark, TC_panel_border,
    TC_accent_soft, TC_action, TC_focus_bg, TC_list, TC_title_text, TC_tx_bg, TC_warn_bg,
    TC_row, TC_row_alert, TC_row_heard, TC_row_tx, TC_row_to_me, TC_row_cq, TC_row_group,
    TC_row_sel_bar, TC_row_alert_text,
    TC_sel_bg, TC_sel_border, TC_sel_text,
    TC_num
} theme_color_t;

uint32_t theme_hex(theme_color_t c);

#define TC(role) lv_color_hex(theme_hex(TC_##role))

typedef struct {
    char     name[32];
    int16_t  radius;
    int16_t  radius_btn;
    int16_t  radius_popup;
    int16_t  border;
    bool     btn_gradient;
    bool     box_gradient;
    bool     focus_solid;
    bool     meter_gradient;
    bool     mono_cluster;
    uint8_t  images;
    int8_t   font;
    int16_t  mark[3];
    int16_t  pill_border;
    bool     mode_box;
    int16_t  btn_gap;
    bool     rule;
    bool     vision;
    uint32_t waterfall[16];
    uint8_t  waterfall_n;
    uint8_t  waterfall_table;
    uint8_t  waterfall_steps;
} theme_shape_t;

/* VISION: every font this many sizes up (the ported dialogs read it) */
#define VISION_FONT_STEPS 3

/* A PNG file as it is (icons), loaded once and kept; NULL when it can't be
 * read. The X6100 theme API; this GUI has no theme images yet. */
const lv_img_dsc_t *theme_png(const char *path);

extern theme_shape_t theme_shape;

#ifdef __cplusplus
}
#endif

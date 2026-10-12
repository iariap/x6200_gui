/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Frequency strip: band on the left, VFO frequency in the middle, then the
 *  locator and the mode on the right; over the locator the TX audio offset of
 *  FT8/FT4/WSPR (the big digits stay on the dial, as WSJT-X). Always shown; the app dialogs start
 *  below it.
 */

#include "freq_strip.h"

#include "cfg/cfg_api.h"
#include "cluster/spots.h"
#include "info.h"
#include "params/params.h"
#include "station.h"
#include "styles.h"
#include "theme.h"
#include "util.h"

#include <stdio.h>

#define QTH_POLL_MS 3000

static char      app[16];
static lv_obj_t *strip, *band_label, *freq_label, *mode_label, *qth_dot, *qth_label, *tx_label;
static char      note[64];
static int32_t   note_freq;
static int32_t   tx_offset = -1;

static void update_cb(Subject *subj, void *user_data) {
    (void)subj;
    (void)user_data;
    uint16_t mhz, khz, hz;
    int32_t  f = cparam_i_get(cfg_fg_freq);

    split_freq(f, &mhz, &khz, &hz);
    if (subject_get_int(cfg_band_split)) {
        uint16_t mhz2, khz2, hz2;
        split_freq(cparam_i_get(cfg_bg_freq), &mhz2, &khz2, &hz2);
        lv_label_set_text_fmt(freq_label, "%i.%03i.%03i / %i.%03i.%03i", mhz, khz, hz, mhz2, khz2, hz2);
    } else {
        lv_label_set_text_fmt(freq_label, "%i.%03i.%03i", mhz, khz, hz);
    }
    const char *band = note[0] && f == note_freq ? note : cluster_band_name(cluster_band_of(f));

    /* The open app in front of the band, so it's clear where the keys go */
    if (app[0] && theme_shape.vision) {   /* big type: on two lines, or it ran into the frequency */
        lv_obj_set_style_text_font(band_label, &sony_24, 0);
        lv_label_set_text_fmt(band_label, "#%06x %s#\n%s", theme_hex(TC_accent), app, band);
    } else if (app[0]) {
        lv_label_set_text_fmt(band_label, "#%06x %s# %s", theme_hex(TC_accent), app, band);
    } else {
        if (theme_shape.vision) lv_obj_set_style_text_font(band_label, &sony_36, 0);
        lv_label_set_text(band_label, band);
    }
    lv_label_set_text(mode_label, info_params_mode_label_get());

    if (tx_offset >= 0) {
        lv_label_set_text_fmt(tx_label, "TX %i Hz", (int)tx_offset);
        lv_obj_clear_flag(tx_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(tx_label, LV_OBJ_FLAG_HIDDEN);
    }
}

/* The GPS locator (green dot) while there is a fix, else the saved QTH */
static void qth_update_cb(lv_timer_t *t) {
    (void)t;
    const char *qth = station_qth();

    lv_label_set_text(qth_label, qth);
    if (qth != params.qth.x) {
        lv_obj_clear_flag(qth_dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(qth_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, lv_align_t align, lv_coord_t x, const lv_font_t *font,
                            uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_align(l, align, x, 0);
    return l;
}

lv_obj_t *freq_strip_init(lv_obj_t *parent) {
    strip = lv_obj_create(parent);
    lv_obj_remove_style_all(strip);
    static lv_style_t style;
    static bool       style_ready;

    if (!style_ready) {
        lv_style_init(&style);
        lv_style_set_bg_opa(&style, LV_OPA_TRANSP);
        lv_style_set_border_width(&style, 0);
        lv_style_set_pad_all(&style, 0);
        style_ready = true;
    }

    lv_obj_add_style(strip, &style, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    bool v = theme_shape.vision;

    band_label = make_label(strip, LV_ALIGN_LEFT_MID, 14, v ? &sony_36 : &sony_24, theme_hex(TC_text_head));
    lv_label_set_recolor(band_label, true);
    /* Centre: frequency, then the mode */
    lv_obj_t *mid = lv_obj_create(strip);
    lv_obj_remove_style_all(mid);
    lv_obj_set_size(mid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_ROW);
    /* VISION: the mode box centred on the big digits (on their baseline it stood too low) */
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_START, v ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_END,
                          v ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(mid, 14, 0);
    lv_obj_align(mid, LV_ALIGN_CENTER, 0, 0);
    freq_label = make_label(mid, LV_ALIGN_DEFAULT, 0, v ? &sony_60 : &sony_36, theme_hex(TC_text));
    mode_label = make_label(mid, LV_ALIGN_DEFAULT, 0, v ? &sony_36 : &sony_24, theme_hex(TC_mode_text));
    lv_obj_set_style_pad_bottom(mode_label, 3, 0);
    if (theme_shape.mode_box) {     /* in a box, as an indicator */
        lv_obj_set_style_bg_color(mode_label, TC(mode_bg), 0);
        lv_obj_set_style_bg_opa(mode_label, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(mode_label, TC(mode_border), 0);
        lv_obj_set_style_border_width(mode_label, 1, 0);
        lv_obj_set_style_radius(mode_label, 3, 0);
        lv_obj_set_style_pad_hor(mode_label, 10, 0);
        lv_obj_set_style_pad_top(mode_label, 2, 0);
        lv_obj_set_style_pad_bottom(mode_label, 6, 0);   /* the box sits on the baseline, as the text did */
        if (v) {
            lv_obj_set_style_radius(mode_label, 8, 0);
            lv_obj_set_style_pad_bottom(mode_label, 4, 0);
            lv_obj_set_style_pad_top(mode_label, 4, 0);
        }
    }

    /* Right: the TX offset (digital apps) over [dot] locator */
    lv_obj_t *col = lv_obj_create(strip);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_align(col, LV_ALIGN_RIGHT_MID, -14, 0);
    tx_label = make_label(col, LV_ALIGN_DEFAULT, 0, v ? &sony_24 : &sony_20, theme_hex(TC_accent));
    lv_obj_add_flag(tx_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *right = lv_obj_create(col);
    lv_obj_remove_style_all(right);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 6, 0);

    qth_dot = lv_obj_create(right);
    lv_obj_remove_style_all(qth_dot);
    lv_obj_set_size(qth_dot, 10, 10);
    lv_obj_set_style_radius(qth_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(qth_dot, TC(ok), 0);
    lv_obj_set_style_bg_opa(qth_dot, LV_OPA_COVER, 0);
    qth_label = make_label(right, LV_ALIGN_DEFAULT, 0, v ? &sony_32 : &sony_28, theme_hex(TC_text));

    subject_subscribe_delayed((Subject *)cfg_fg_freq, update_cb, NULL);
    subject_subscribe_delayed((Subject *)cfg_bg_freq, update_cb, NULL);
    subject_subscribe_delayed((Subject *)cfg_band_split, update_cb, NULL);
    subject_subscribe_delayed((Subject *)cfg_cur_mode, update_cb, NULL);
    update_cb(NULL, NULL);
    qth_update_cb(NULL);
    lv_timer_create(qth_update_cb, QTH_POLL_MS, NULL);
    return strip;
}

void freq_strip_set_label(const char *text, int32_t freq_hz) {
    snprintf(note, sizeof(note), "%s", text);
    note_freq = freq_hz;
    if (strip) update_cb(NULL, NULL);
}

void freq_strip_set_app(const char *name) {
    snprintf(app, sizeof(app), "%s", name ? name : "");
    if (strip) update_cb(NULL, NULL);
}

void freq_strip_set_offset(int32_t offset_hz) {
    tx_offset = offset_hz;
    if (strip) update_cb(NULL, NULL);
}

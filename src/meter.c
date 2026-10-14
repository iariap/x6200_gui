/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "meter.h"
#include "theme.h"
#include "styles.h"
#include "events.h"
#include "params/params.h"
#include "spectrum.h"
#include "util.h"

#include <math.h>
#include <stdio.h>

#define NUM_ITEMS   7
#define METER_PEAK_HOLD 1500
#define METER_PEAK_SPEED 20

static int16_t          min_db = S1;
static int16_t          max_db = S9_40;

static int16_t          meter_db = S1;
static int16_t          meter_db_raw = S1;
static float            noise_level = S_MIN;

static int16_t          meter_peak = S1;
static int64_t          meter_peak_time;
static int64_t          now;

static bool             pre=false;
static bool             att=false;

static lv_obj_t         *obj;

typedef struct {
    char    *label;
    int16_t db;
} s_item_t;

static s_item_t s_items[NUM_ITEMS] = {
    { .label = "S1",    .db = S1 },
    { .label = "3",     .db = S3 },
    { .label = "5",     .db = S5 },
    { .label = "7",     .db = S7 },
    { .label = "9",     .db = S9 },
    { .label = "+20",   .db = S9_20 },
    { .label = "+40",   .db = S9_40 }
};

static void on_bool_value_change(Subject *subj, void *user_data) {
    *(bool*)user_data = subject_get_int(subj);
}

/* --- Needle S-meter (Settings > Display: "S-meter needle") ----------------
 * A dial whose pivot sits below the box, so only the top of the arc shows:
 * S1..S9 take 70% of the arc, +20/+40 the rest (the red zone of a real meter,
 * drawn thicker). Idea and geometry from the X6100 firmware by IU1WXY,
 * adapted to this GUI's palette and fonts. */

#define NEEDLE_A0     (-32)         /* degrees from vertical, left end */
#define NEEDLE_A1     32
#define NEEDLE_S9_U   0.7f          /* fraction of the arc used by S1..S9 */

static float needle_u(float db) {
    float u = db <= S9 ? (db - S1) / (float)(S9 - S1) * NEEDLE_S9_U
                       : NEEDLE_S9_U + (db - S9) / (float)(S9_40 - S9) * (1.0f - NEEDLE_S9_U);

    return LV_CLAMP(0.0f, u, 1.0f);
}

static lv_point_t needle_pt(lv_point_t c, float a_deg, lv_coord_t r) {
    float      a = a_deg * (float)M_PI / 180.0f;
    lv_point_t p = { c.x + (lv_coord_t)lroundf(r * sinf(a)),
                     c.y - (lv_coord_t)lroundf(r * cosf(a)) };

    return p;
}

static void needle_label(lv_draw_ctx_t *draw_ctx, lv_draw_label_dsc_t *dsc,
                         lv_point_t c, float a, lv_coord_t r, const char *txt) {
    lv_point_t sz;
    lv_point_t l = needle_pt(c, a, r);
    lv_area_t  area;

    lv_txt_get_size(&sz, txt, dsc->font, 0, 0, LV_COORD_MAX, 0);

    area.x1 = l.x - sz.x / 2;
    area.y1 = l.y - sz.y / 2;
    area.x2 = area.x1 + sz.x;
    area.y2 = area.y1 + sz.y;

    lv_draw_label(draw_ctx, dsc, &area, txt, NULL);
}

static void needle_draw(lv_obj_t *obj, lv_draw_ctx_t *draw_ctx) {
    lv_coord_t          w = lv_obj_get_width(obj);
    lv_coord_t          r = w * 76 / 100;      /* wide and flat: 198 px on a 260 px box */
    lv_point_t          c = { obj->coords.x1 + w / 2, obj->coords.y1 + 24 + r };
    lv_draw_arc_dsc_t   arc_dsc;
    lv_draw_line_dsc_t  line_dsc;
    lv_draw_label_dsc_t label_dsc;
    float               span = NEEDLE_A1 - NEEDLE_A0;

    /* LVGL arcs: 0 degrees at 3 o'clock, clockwise; straight up is 270 */
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(theme_hex(TC_meter_bar));
    arc_dsc.width = 2;
    lv_draw_arc(draw_ctx, &arc_dsc, &c, r, 270 + NEEDLE_A0,
                270 + NEEDLE_A0 + (int)(span * NEEDLE_S9_U));

    arc_dsc.color = lv_color_hex(theme_hex(TC_meter_over));
    arc_dsc.width = 5;
    lv_draw_arc(draw_ctx, &arc_dsc, &c, r + 2, 270 + NEEDLE_A0 + (int)(span * NEEDLE_S9_U),
                270 + NEEDLE_A1);

    lv_draw_line_dsc_init(&line_dsc);
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = &sony_16;

    /* S1..S9: a tick per unit, longer tick and a number on the odd ones */
    for (int s = 1; s <= 9; s++) {
        float      a   = NEEDLE_A0 + span * needle_u(S1 + (s - 1) * (S9 - S1) / 9.0f);
        bool       big = (s % 2) != 0;
        lv_point_t p1  = needle_pt(c, a, r);
        lv_point_t p2  = needle_pt(c, a, r - (big ? 9 : 5));

        line_dsc.color = lv_color_hex(theme_hex(TC_meter_bar));
        line_dsc.width = big ? 2 : 1;
        lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);

        if (big) {
            char txt[4];

            snprintf(txt, sizeof(txt), s == 1 ? "S1" : "%d", s);
            label_dsc.color = lv_color_white();
            needle_label(draw_ctx, &label_dsc, c, a, r + 13, txt);
        }
    }

    /* S9 +10..+40: the red zone */
    for (int d = 10; d <= 40; d += 10) {
        float      a   = NEEDLE_A0 + span * needle_u(S9 + d);
        bool       big = (d % 20) == 0;
        lv_point_t p1  = needle_pt(c, a, r + 2);
        lv_point_t p2  = needle_pt(c, a, r - (big ? 10 : 6));

        line_dsc.color = lv_color_hex(theme_hex(TC_meter_over));
        line_dsc.width = big ? 2 : 1;
        lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);

        if (big) {
            char txt[4];

            snprintf(txt, sizeof(txt), "+%d", d);
            label_dsc.color = lv_color_hex(theme_hex(TC_meter_low));
            needle_label(draw_ctx, &label_dsc, c, a, r + 14, txt);
        }
    }

    /* The current value, under the dial */
    char txt[8];

    snprintf(txt, sizeof(txt), "%d", (int)meter_db);

    label_dsc.font  = &sony_18;
    label_dsc.color = lv_color_white();
    needle_label(draw_ctx, &label_dsc, c, 0.0f, r - 30, txt);

    /* The needle, from the pivot to just past the arc */
    lv_point_t tip = needle_pt(c, NEEDLE_A0 + span * needle_u(meter_db), r + 2);

    line_dsc.color     = lv_color_white();
    line_dsc.width     = 2;
    line_dsc.round_end = 1;
    lv_draw_line(draw_ctx, &line_dsc, &c, &tip);
}

static void meter_draw_cb(lv_event_t * e) {
    lv_obj_t            *obj = lv_event_get_target(e);
    lv_draw_ctx_t       *draw_ctx = lv_event_get_draw_ctx(e);
    lv_draw_rect_dsc_t  rect_dsc;
    lv_draw_label_dsc_t label_dsc;
    lv_area_t           area;

    if (params.smeter_needle.x) {
        needle_draw(obj, draw_ctx);
        return;
    }

    lv_coord_t x1 = obj->coords.x1 + 7;
    lv_coord_t y1 = obj->coords.y1 + 17;

    lv_coord_t w = lv_obj_get_width(obj) - 80;
    // lv_coord_t h = lv_obj_get_height(obj) - 1;

    uint8_t     slice_db = 3;
    uint8_t     slices_total = (max_db - min_db) / slice_db;
    uint8_t     slice_w = w / slices_total;
    uint8_t     slice_spacing = slice_w * 2 / 10;

    /* Rects */

    lv_draw_rect_dsc_init(&rect_dsc);

    rect_dsc.bg_opa = LV_OPA_80;

    uint32_t count = (meter_db - min_db + slice_db) / slice_db;
    count = LV_MIN(count, slices_total);

    area.y1 = y1 - 5;
    area.y2 = y1 + 32;

    int16_t db = s_items[0].db;

    for (uint16_t i = 0; i < count; i++) {
        if (db <= noise_level) {
            rect_dsc.bg_color = lv_color_hex(theme_hex(TC_meter_weak));
        } else if (db <= -73) {
            rect_dsc.bg_color = lv_color_hex(theme_hex(TC_meter_bar));
        } else if (db <= -53) {
            rect_dsc.bg_color = lv_color_hex(theme_hex(TC_meter_low));
        } else {
            rect_dsc.bg_color = lv_color_hex(theme_hex(TC_meter_over));
        }
        area.x1 = x1 + 30 + i * slice_w - slice_w / 2 + slice_spacing / 2;
        area.x2 = area.x1 + slice_w - slice_spacing;

        lv_draw_rect(draw_ctx, &rect_dsc, &area);

        db += slice_db;
    }

    /* Peak */
    if (meter_peak > meter_db) {
        area.x1 = x1 + 30  - slice_w / 2 + slice_w * ((meter_peak - min_db) / slice_db);
        area.x2 = area.x1 + slice_w - slice_spacing;
        rect_dsc.bg_opa = LV_OPA_50;
        rect_dsc.bg_color = lv_color_hex(theme_hex(TC_meter_bar));
        lv_draw_rect(draw_ctx, &rect_dsc, &area);
    }

    /* Labels */

    lv_draw_label_dsc_init(&label_dsc);

    label_dsc.color = lv_color_white();
    label_dsc.font = &sony_22;

    area.x1 = x1;
    area.x2 = x1 + 20;
    area.y1 = y1 + 5;
    area.y2 = area.y1 + 18;

    lv_point_t label_size;

    for (uint8_t i = 0; i < NUM_ITEMS; i++) {
        char    *label = s_items[i].label;
        int16_t db = s_items[i].db;

        lv_txt_get_size(&label_size, label, label_dsc.font, 0, 0, LV_COORD_MAX, 0);

        area.x1 = x1 + 30 + slice_w * ((db  - min_db) / slice_db) - label_size.x / 2;
        area.x2 = area.x1 + label_size.x;

        lv_draw_label(draw_ctx, &label_dsc, &area, label, NULL);
    }
}

static void tx_cb(lv_event_t * e) {
    if (obj) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void rx_cb(lv_event_t * e) {
    if (obj) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}


lv_obj_t * meter_init(lv_obj_t * parent) {
    obj = lv_obj_create(parent);

    lv_obj_add_style(obj, &meter_style, 0);

    lv_obj_add_event_cb(obj, tx_cb, EVENT_RADIO_TX, NULL);
    lv_obj_add_event_cb(obj, rx_cb, EVENT_RADIO_RX, NULL);
    lv_obj_add_event_cb(obj, meter_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);

    subject_add_delayed_observer(cfg_cur.pre, on_bool_value_change, &pre);
    on_bool_value_change(cfg_cur.pre, &pre);
    subject_add_delayed_observer(cfg_cur.att, on_bool_value_change, &att);
    on_bool_value_change(cfg_cur.att, &att);

    return obj;
}

void meter_update(int16_t db, float beta) {
    // TODO: make adjustable
    db -= 8;
    noise_level = spectrum_get_min();

    if (db < min_db) {
        db = min_db;
    } else if (db > max_db) {
        db = max_db;
    }
    meter_db_raw = db;
    now = get_time();
    if (db > meter_peak) {
        meter_peak = db;
        meter_peak_time = now;
    } else if (now - meter_peak_time > METER_PEAK_HOLD) {
        meter_peak -= (now - meter_peak_time - METER_PEAK_HOLD) * METER_PEAK_SPEED / 1000;
    }
    meter_db = meter_db * beta + db * (1.0f - beta);
    event_send(obj, LV_EVENT_REFRESH, NULL);
}

int16_t meter_get_raw_db() {
    return meter_db_raw;
}

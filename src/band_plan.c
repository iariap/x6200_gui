/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - IARU Region 1 band plan strip (see band_plan.h).
 */

#include "band_plan.h"

#include "params/params.h"
#include "styles.h"

#include <stddef.h>
#include <string.h>

#define OPA LV_OPA_80       /* the spectrum shows through a little, labels stay readable */

/* Strip colours and labels; the X6100 takes these from its theme */
static const uint32_t colors[] = { 0x3366CC, 0x33AA55, 0x888888, 0xCC8800 };
static const char *const labels[][2] = {
    { "CW", NULL }, { "DIG", NULL }, { "ALL MODES / SSB", "SSB" }, { "BCN", NULL },
};

typedef struct {
    lv_obj_t *box;
    lv_obj_t *label;
} seg_t;

static seg_t   segs[BAND_PLAN_MAX];
static bool    ready;
static int32_t span_center, span_half;

static void redraw(void) {
    bp_visible_t vis[BAND_PLAN_MAX];
    int          n = 0;

    if (ready && params.band_plan.x) {
        n = band_plan_segments(span_center, span_half, vis, BAND_PLAN_MAX);
    }

    for (int i = 0; i < BAND_PLAN_MAX; i++) {
        seg_t *s = &segs[i];

        if (!s->box) {
            continue;
        }

        if (i >= n) {
            lv_obj_add_flag(s->box, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const char *text = NULL;

        lv_obj_set_x(s->box, vis[i].x);
        lv_obj_set_width(s->box, vis[i].w);
        lv_obj_set_style_bg_color(s->box, lv_color_hex(colors[vis[i].type]), 0);

        for (int k = 0; k < 2 && labels[vis[i].type][k] && !text; k++) {
            const char *t = labels[vis[i].type][k];

            if (lv_txt_get_width(t, strlen(t), &sony_16, 0, LV_TEXT_FLAG_NONE) + 6 <= vis[i].w) {
                text = t;
            }
        }

        if (text) {
            lv_label_set_text_static(s->label, text);
            lv_obj_center(s->label);
            lv_obj_clear_flag(s->label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s->label, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_clear_flag(s->box, LV_OBJ_FLAG_HIDDEN);
    }
}

void band_plan_init(lv_obj_t *parent, lv_coord_t y) {
    for (int i = 0; i < BAND_PLAN_MAX; i++) {
        lv_obj_t *b = lv_obj_create(parent);

        lv_obj_remove_style_all(b);
        lv_obj_set_y(b, y);
        lv_obj_set_height(b, BAND_PLAN_H);
        lv_obj_set_style_bg_opa(b, OPA, 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
        segs[i].box = b;

        lv_obj_t *l = lv_label_create(b);

        lv_obj_set_style_text_font(l, &sony_16, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        segs[i].label = l;
    }

    ready = true;
    redraw();
}

void band_plan_set_y(lv_coord_t y) {
    for (int i = 0; i < BAND_PLAN_MAX; i++) {
        if (segs[i].box) {
            lv_obj_set_y(segs[i].box, y);
        }
    }
}

void band_plan_update(int32_t center, int32_t half_width) {
    if (center == span_center && half_width == span_half) {
        return;
    }

    span_center = center;
    span_half   = half_width;
    redraw();
}

void band_plan_refresh(void) {
    redraw();
}

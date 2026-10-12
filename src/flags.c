/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - country flags (see flags.h).
 */

#include "flags.h"

#include "flags_data.h"

#include <stdlib.h>
#include <string.h>

#define FLAGS_MAX   (int) (sizeof(flag_px) / sizeof(flag_px[0]))

typedef struct {
    lv_img_dsc_t dsc;
    uint16_t    *pixels;        /* NULL until the flag is first asked for */
} flag_t;

static flag_t flags[FLAGS_MAX];

uint16_t flags_rgb888_to_565(uint32_t rgb) {
    uint16_t r = (rgb >> 16) & 0xFF;
    uint16_t g = (rgb >> 8) & 0xFF;
    uint16_t b = rgb & 0xFF;

    return (uint16_t) (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

int flags_count(void) {
    return FLAGS_MAX;
}

bool flags_index_of_prefix(const char *prefix, int *index) {
    if (!prefix || !prefix[0]) {
        return false;
    }

    for (size_t i = 0; i < sizeof(flag_of) / sizeof(flag_of[0]); i++) {
        if (!strcmp(flag_of[i].prefix, prefix)) {
            if (index) {
                *index = (int) flag_of[i].flag;
            }

            return true;
        }
    }

    return false;
}

const lv_img_dsc_t *flags_image(int index) {
    if (index < 0 || index >= FLAGS_MAX) {
        return NULL;
    }

    flag_t *f = &flags[index];

    if (!f->pixels) {
        f->pixels = malloc(FLAG_W * FLAG_H * sizeof(uint16_t));

        if (!f->pixels) {
            return NULL;
        }

        for (int i = 0; i < FLAG_W * FLAG_H; i++) {
            const uint8_t *p = &flag_px[index][i * 3];

            f->pixels[i] = flags_rgb888_to_565(((uint32_t) p[0] << 16) |
                                               ((uint32_t) p[1] << 8) | p[2]);
        }

        f->dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        f->dsc.header.w  = FLAG_W;
        f->dsc.header.h  = FLAG_H;
        f->dsc.data_size = FLAG_W * FLAG_H * sizeof(uint16_t);
        f->dsc.data      = (const uint8_t *) f->pixels;
    }

    return &f->dsc;
}

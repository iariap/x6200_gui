/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - IARU Region 1 band plan data and geometry.
 *
 *  Frequencies from the X6100 firmware by IU1WXY (src/band_plan.c); the maths
 *  is kept here, free of LVGL, so the tests can check it.
 */

#include "band_plan_data.h"

#include <stddef.h>

typedef struct {
    int32_t   start, end;   /* Hz */
    bp_type_t type;
} bp_seg_t;

/* IARU Region 1, adjacent ranges of the same type merged (160 m .. 6 m) */
static const bp_seg_t plan[] = {
    {  1810000,  1838000, BP_CW  }, {  1838000,  1843000, BP_DIG }, {  1843000,  2000000, BP_ALL },
    {  3500000,  3570000, BP_CW  }, {  3570000,  3600000, BP_DIG }, {  3600000,  3800000, BP_ALL },
    {  7000000,  7040000, BP_CW  }, {  7040000,  7050000, BP_DIG }, {  7050000,  7200000, BP_ALL },
    { 10100000, 10130000, BP_CW  }, { 10130000, 10150000, BP_DIG },
    { 14000000, 14070000, BP_CW  }, { 14070000, 14099000, BP_DIG }, { 14099000, 14101000, BP_BCN },
    { 14101000, 14112000, BP_DIG }, { 14112000, 14350000, BP_ALL },
    { 18068000, 18095000, BP_CW  }, { 18095000, 18109000, BP_DIG }, { 18109000, 18111000, BP_BCN },
    { 18111000, 18120000, BP_DIG }, { 18120000, 18168000, BP_ALL },
    { 21000000, 21070000, BP_CW  }, { 21070000, 21120000, BP_DIG }, { 21120000, 21149000, BP_CW  },
    { 21149000, 21151000, BP_BCN }, { 21151000, 21450000, BP_ALL },
    { 24890000, 24915000, BP_CW  }, { 24915000, 24929000, BP_DIG }, { 24929000, 24931000, BP_BCN },
    { 24931000, 24990000, BP_ALL },
    /* CB 27 MHz (issue #169): los cuatro grids de 40 canales, entre 12 m y 10 m.
     * Cada segmento es el rango completo del grid (canal 1 .. canal 40); el hueco
     * de 10 kHz entre grids queda sin pintar. */
    { 26065000, 26505000, BP_CB  },   /* CB A */
    { 26515000, 26955000, BP_CB  },   /* CB B */
    { 26965000, 27405000, BP_CB  },   /* CB C */
    { 27415000, 27855000, BP_CB  },   /* CB D */
    { 28000000, 28070000, BP_CW  }, { 28070000, 28190000, BP_DIG }, { 28190000, 28225000, BP_BCN },
    { 28225000, 29700000, BP_ALL },
    { 50000000, 50100000, BP_CW  }, { 50100000, 50300000, BP_ALL }, { 50300000, 50400000, BP_DIG },
    { 50400000, 50500000, BP_BCN }, { 50500000, 52000000, BP_ALL },
};

bp_type_t band_plan_type_at(int32_t freq) {
    for (size_t i = 0; i < sizeof(plan) / sizeof(plan[0]); i++) {
        const bp_seg_t *p = &plan[i];

        /* Rango [inicio, fin): 50.100 kHz ya es el segmento siguiente (medido en la
         * franja: > 50.100 pinta ALL, no CW). */
        if (freq >= p->start && freq < p->end) {
            return p->type;
        }
    }

    return (bp_type_t) BP_NONE;
}

int band_plan_segments(int32_t center, int32_t half_width, bp_visible_t *out, int max_out) {
    int n = 0;

    if (!out || max_out <= 0 || half_width <= 0) {
        return 0;
    }

    int32_t left = center - half_width;
    int64_t span = 2 * (int64_t) half_width;

    for (size_t i = 0; i < sizeof(plan) / sizeof(plan[0]) && n < max_out; i++) {
        const bp_seg_t *p = &plan[i];
        int64_t         a = (int64_t)(p->start - left) * BAND_PLAN_W / span;
        int64_t         b = (int64_t)(p->end - left) * BAND_PLAN_W / span;

        if (b <= 0 || a >= BAND_PLAN_W) {
            continue;                       /* out of the displayed span */
        }

        if (a < 0) {
            a = 0;
        }

        if (b > BAND_PLAN_W) {
            b = BAND_PLAN_W;
        }

        if (b <= a) {
            b = a + 1;                      /* keep a hairline visible */
        }

        out[n].x    = (int32_t) a;
        out[n].w    = (int32_t) (b - a);
        out[n].type = p->type;
        n++;
    }

    return n;
}

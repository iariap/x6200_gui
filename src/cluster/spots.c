/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - band lookup (see spots.h).
 */

#include "spots.h"

#include <stddef.h>

typedef struct {
    const char *name;
    int32_t     start;
    int32_t     stop;
} band_t;

/* IARU Region 1, HF and 6 m */
static const band_t bands[] = {
    { "160m",   1810000,   2000000 },
    { "80m",    3500000,   3800000 },
    { "60m",    5351500,   5366500 },
    { "40m",    7000000,   7200000 },
    { "30m",   10100000,  10150000 },
    { "20m",   14000000,  14350000 },
    { "17m",   18068000,  18168000 },
    { "15m",   21000000,  21450000 },
    { "12m",   24890000,  24990000 },
    { "10m",   28000000,  29700000 },
    { "6m",    50000000,  52000000 },
};

int cluster_band_of(int32_t freq_hz) {
    for (size_t i = 0; i < sizeof(bands) / sizeof(bands[0]); i++) {
        if (freq_hz >= bands[i].start && freq_hz <= bands[i].stop) {
            return (int) i;
        }
    }

    return -1;
}

const char *cluster_band_name(int band) {
    if (band < 0 || band >= (int) (sizeof(bands) / sizeof(bands[0]))) {
        return NULL;
    }

    return bands[band].name;
}

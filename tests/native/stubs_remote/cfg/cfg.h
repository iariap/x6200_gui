/* Stub for src/cfg/cfg.h: the configuration items src/cfg/settings_manager.cpp binds to. */
#pragma once

#include <stdint.h>

#include "subjects.h"
#include "../lvgl/lvgl.h"

typedef struct {
    Subject *val;
} cfg_item_t;

typedef struct {
    Subject *fg_freq, *bg_freq, *mode, *agc, *att, *pre, *lo_offset;
    struct {
        Subject *bw, *low, *high;
    } filter;
} cfg_cur_t;

typedef struct {
    cfg_item_t vol, sql, nb, nb_level, nb_width, nr, nr_level, band_id, pwr, cw_decoder;
} cfg_t;

#ifdef __cplusplus
extern "C" {
#endif

extern cfg_t     cfg;
extern cfg_cur_t cfg_cur;

#ifdef __cplusplus
}
#endif

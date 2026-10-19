/* Stub de params/params.h para el test determinista del waterfall (OPT-2, #148).
 * Declara solo lo que src/waterfall.c lee: los cuatro parametros del waterfall y
 * los subjects de cfg_cur que registra como observers. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct Subject Subject;

typedef struct {
    bool x;
} params_bool_t;

typedef struct {
    params_bool_t waterfall_smooth_scroll;
    params_bool_t waterfall_center_line;
    params_bool_t waterfall_auto_min;
    params_bool_t waterfall_auto_max;
    params_bool_t waterfall_battery;
} params_t;

extern params_t params;

/* Minimo de cfg_cur que waterfall.c toca: el VFO, el ancho de FFT y el grid de
 * la banda (min/max). */
typedef struct {
    Subject *val;
} cfg_grid_value_t;

typedef struct {
    cfg_grid_value_t min;
    cfg_grid_value_t max;
} cfg_grid_t;

typedef struct {
    cfg_grid_t grid;
} cfg_band_t;

typedef struct {
    Subject   *fg_freq;
    Subject   *fft_width;
    cfg_band_t *band;
} cfg_cur_t;

extern cfg_cur_t cfg_cur;

void *subject_add_observer_and_call(Subject *subj, void (*fn)(Subject *, void *), void *user_data);
int32_t subject_get_int(Subject *subj);
void subject_set_int(Subject *subj, int32_t val);

/* El test crea los subjects que cfg_cur referencia. */
Subject *test_subject_create(int32_t initial);

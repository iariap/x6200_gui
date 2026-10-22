/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

lv_obj_t * spectrum_init(lv_obj_t * parent);
void spectrum_data(float *data_buf, uint16_t size, bool tx);
void spectrum_min_max_reset();

/* Banda pasante RX a escala (issue #173, PR 1).
 *
 * Geometria en pixeles de la ventana de filtro sobre el area de espectro, a
 * partir de los bordes reales de RF que publica cfg/mode.c::update_real_filters()
 * en cfg_cur.filter.real.from/to y del ancho mostrado (cfg_cur.fft_width).
 *
 * Las coordenadas son RELATIVAS al borde izquierdo del area dibujable (el
 * llamador les suma su x1): con `w` px utiles, el centro del espectro cae en
 * w/2 y
 *
 *     xA = w/2 + round(from * w / span)
 *     xB = w/2 + round(to   * w / span)
 *     left = min(xA, xB); right = max(xA, xB)
 *
 * con redondeo al entero mas cercano y, en los medios, alejandose del cero
 * (documentado; la tolerancia del golden es +/-1 px por esta regla). `left` y
 * `right` se recortan a [0, w]. `visible` es false cuando no hay interseccion
 * con el area dibujable o cuando `span <= 0` (nunca se divide por cero).
 *
 * Es pura y sin estado: la usan el espectro y los ticks del waterfall, y el test
 * nativo la verifica con el golden de #173. */
typedef struct {
    bool    visible;
    int32_t left;
    int32_t right;
} spectrum_filter_band_t;

/* Division entera con redondeo al entero mas cercano; en los medios, alejandose
 * del cero. `den` es siempre > 0 (span > 0, garantizado por el llamador). */
static inline int32_t spectrum_round_div(int64_t num, int32_t den) {
    int64_t half = den / 2;

    if (num >= 0) {
        return (int32_t) ((num + half) / den);
    }

    return (int32_t) ((num - half) / den);
}

/* Implementacion inline: la comparten el espectro y los ticks del waterfall
 * (cada unidad de traduccion su copia), sin acoplar el modulo del waterfall al
 * link del espectro. El test nativo la verifica con el golden de #173. */
static inline spectrum_filter_band_t spectrum_filter_band(int32_t from, int32_t to, int32_t span, int32_t w) {
    spectrum_filter_band_t band = { false, 0, 0 };

    if (span <= 0 || w <= 0) {
        return band;
    }

    int32_t x0 = w / 2;
    int32_t xa = x0 + spectrum_round_div((int64_t) from * w, span);
    int32_t xb = x0 + spectrum_round_div((int64_t) to * w, span);

    int32_t left  = (xa < xb) ? xa : xb;
    int32_t right = (xa < xb) ? xb : xa;

    if (right < 0 || left > w) {
        return band;    /* sin interseccion con el area dibujable */
    }

    if (left < 0) {
        left = 0;
    }
    if (right > w) {
        right = w;
    }

    band.visible = true;
    band.left    = left;
    band.right   = right;

    return band;
}

/* Cadencia del redibujo del espectro: dibuja 1 de cada `frames` refrescos (el
 * mismo mecanismo del modo Ahorro del waterfall, src/waterfall.c). Lo fija el
 * perfil visual (visual_policy_spectrum_apply); `frames` 0 se ignora y 1 vuelve a
 * dibujar siempre. */
void spectrum_set_refresh_period(uint8_t frames);

float spectrum_get_min();
void spectrum_update_max(float db);
void spectrum_update_min(float db);
void spectrum_clear();
// void spectrum_update_filters();
// void spectrum_update_factor();

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Modelo puro del editor DFL (issue #173, PR 2).
 *
 *  Aca vive TODO lo que el editor de filtro puede decidir sin LVGL y sin
 *  Subjects: los pasos de edicion de src/vol.c, la cuantizacion, el mapeo de
 *  Hz del filtro a pixeles del grafico esquematico, los objetivos de LOW/HIGH
 *  cuando se edita el BW y la legalidad de esos objetivos. El dialogo
 *  (src/dfl_inline.c) es el unico consumidor; el test nativo
 *  (tests/native/test_dfl_editor.c) fija el golden de geometria y las reglas.
 *
 *  Es un ESPEJO de la logica vigente del firmware, no una segunda fuente de
 *  verdad: los pasos salen de src/vol.c, el mapeo de BW de
 *  src/cfg/mode.c::on_cur_filter_bw_change() y el limite de
 *  src/cfg/mode.h::MAX_FILTER_FREQ. Si esa logica cambia, esto cambia con ella.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cfg/mode.h"

/* Pasos de edicion (src/vol.c): LOW 10 Hz siempre; HIGH 50 Hz y 10 Hz en
 * CW/CWR; BW 20 Hz. */
#define DFL_STEP_LOW         10
#define DFL_STEP_HIGH_SSB    50
#define DFL_STEP_HIGH_CW     10
#define DFL_STEP_BW          20

/* Parametro del editor sobre el que actua la rueda. La fila tocada lo
 * selecciona (src/dfl_inline.c) y el encoder ajusta el seleccionado. */
typedef enum {
    DFL_SEL_LOW = 0,
    DFL_SEL_HIGH,
    DFL_SEL_BW,
    DFL_SEL_COUNT,
} dfl_sel_t;

/* LOW no es editable en AM/SAM/NFM/WFM: es la misma regla que
 * cfg_mode_set_low_filter() (devuelve 0 sin tocar el subject). */
static inline bool dfl_mode_low_editable(x6200_mode_t mode) {
    switch (mode) {
        case x6200_mode_am:
        case x6200_mode_sam:
        case x6200_mode_nfm:
        case x6200_mode_wfm:
            return false;
        default:
            return true;
    }
}

static inline bool dfl_mode_is_cw(x6200_mode_t mode) {
    return mode == x6200_mode_cw || mode == x6200_mode_cwr;
}

/* AM/SAM/NFM/WFM: la respuesta es simetrica (+/- high) y LOW queda en 0. */
static inline bool dfl_mode_is_symmetric(x6200_mode_t mode) {
    switch (mode) {
        case x6200_mode_am:
        case x6200_mode_sam:
        case x6200_mode_nfm:
        case x6200_mode_wfm:
            return true;
        default:
            return false;
    }
}

static inline int32_t dfl_step_low(x6200_mode_t mode) {
    (void) mode;
    return DFL_STEP_LOW;
}

static inline int32_t dfl_step_high(x6200_mode_t mode) {
    return dfl_mode_is_cw(mode) ? DFL_STEP_HIGH_CW : DFL_STEP_HIGH_SSB;
}

static inline int32_t dfl_step_bw(void) {
    return DFL_STEP_BW;
}

/* Paso de la rueda segun el parametro seleccionado: son los MISMOS pasos de
 * src/vol.c, no una tabla nueva. */
static inline int32_t dfl_sel_step(dfl_sel_t sel, x6200_mode_t mode) {
    switch (sel) {
        case DFL_SEL_LOW:
            return dfl_step_low(mode);
        case DFL_SEL_HIGH:
            return dfl_step_high(mode);
        case DFL_SEL_BW:
            return dfl_step_bw();
        default:
            return 0;
    }
}

/* Seleccion inicial al abrir el editor: LOW en los modos donde es editable
 * (SSB/CW, el borde que mas se ajusta). En AM/SAM/NFM/WFM LOW no es editable
 * (queda en 0 y el setter lo rechaza), asi que el default es HIGH: la rueda
 * arranca en el borde util y BW queda a un toque. */
static inline dfl_sel_t dfl_default_sel(x6200_mode_t mode) {
    return dfl_mode_low_editable(mode) ? DFL_SEL_LOW : DFL_SEL_HIGH;
}

/* Cuantiza al paso con la misma semantica que util.cpp::align_int()
 * (x - x % step, truncando hacia cero): los valores guardados que no son
 * multiplos NO se tocan al abrir, solo se cuantizan tras una modificacion. */
static inline int32_t dfl_quantize(int32_t val, int32_t step) {
    if (step <= 0) {
        return val;
    }
    return val - (val % step);
}

/* --- Grafico esquematico --------------------------------------------------
 * El eje es la frecuencia DEL FILTRO (Hz de low/high), con el cero visible, no
 * las coordenadas del waterfall. El dominio es ADAPTATIVO (issue #179, C.3): en
 * vez de dibujar siempre 0..10000 (que hace impracticable ver un filtro de
 * 2-3 kHz), se elige el menor de los dominios "agradables" 1000 / 2000 / 5000 /
 * 10000 Hz que contenga los DOS limites. En SSB el dominio es [0, M]; en CW/CWR
 * y en los modos simetricos (AM/SAM/NFM/WFM) es [-M, +M], incluso con LOW
 * negativo. Golden del contrato: USB-D 225..3525 -> 0..5000 Hz. */
typedef struct {
    int32_t min_hz;
    int32_t max_hz;
} dfl_domain_t;

/* Los cuatro dominios del contrato, de menor a mayor. */
#define DFL_DOMAIN_COUNT 4

static inline int32_t dfl_domain_at(int i) {
    switch (i) {
        case 0:
            return 1000;
        case 1:
            return 2000;
        case 2:
            return 5000;
        default:
            return MAX_FILTER_FREQ;   /* 10000 */
    }
}

/* Menor dominio que contiene ambos limites. Si ninguno alcanza (no deberia:
 * MAX_FILTER_FREQ es el tope del firmware) devuelve el tope. */
static inline int32_t dfl_fit_span(int32_t low, int32_t high) {
    int32_t m = (low < 0) ? -low : low;
    int32_t h = (high < 0) ? -high : high;

    if (h > m) {
        m = h;
    }

    for (int i = 0; i < DFL_DOMAIN_COUNT; i++) {
        if (dfl_domain_at(i) >= m) {
            return dfl_domain_at(i);
        }
    }

    return MAX_FILTER_FREQ;
}

/* Arma el dominio del modo a partir de la magnitud `span_m`. */
static inline dfl_domain_t dfl_domain_span(x6200_mode_t mode, int32_t span_m) {
    dfl_domain_t d;

    if (dfl_mode_is_cw(mode) || dfl_mode_is_symmetric(mode)) {
        d.min_hz = -span_m;
        d.max_hz = span_m;
    } else {
        d.min_hz = 0;
        d.max_hz = span_m;
    }

    return d;
}

/* Dominio adaptativo de un filtro concreto (lo que se fija al entrar o al
 * cambiar de modo). */
static inline dfl_domain_t dfl_domain_fit(x6200_mode_t mode, int32_t low, int32_t high) {
    return dfl_domain_span(mode, dfl_fit_span(low, high));
}

/* Histéresis de ampliacion: NUNCA reduce. Si el filtro se fue fuera del dominio
 * fijado, salta al menor dominio que lo contenga; si sigue adentro, conserva el
 * actual (asi el grafico no salta en cada detent). */
static inline int32_t dfl_domain_grow(int32_t span_m, int32_t low, int32_t high) {
    int32_t need = dfl_fit_span(low, high);

    return (need > span_m) ? need : span_m;
}

/* Redondeo al entero mas cercano (el numerador es >= 0: hz >= min_hz). */
static inline int32_t dfl_round_div(int64_t num, int32_t den) {
    if (den <= 0) {
        return (int32_t) num;
    }
    return (int32_t) ((num + den / 2) / den);
}

/* Hz del filtro -> x en el grafico (relativo a x0, ancho w). Se recorta al
 * rango dibujable; nunca divide por cero (w <= 0 o dominio vacio -> x0). */
static inline int32_t dfl_graph_x(int32_t hz, dfl_domain_t d, int32_t x0, int32_t w) {
    if (w <= 0 || d.max_hz <= d.min_hz) {
        return x0;
    }

    int64_t span = (int64_t) d.max_hz - d.min_hz;
    int32_t x    = x0 + dfl_round_div((int64_t) (hz - d.min_hz) * w, (int32_t) span);

    if (x < x0) {
        x = x0;
    }
    if (x > x0 + w) {
        x = x0 + w;
    }

    return x;
}

/* x en el grafico -> Hz del filtro (inverso de dfl_graph_x, sin cuantizar). */
static inline int32_t dfl_graph_hz(int32_t x, dfl_domain_t d, int32_t x0, int32_t w) {
    if (w <= 0 || d.max_hz <= d.min_hz) {
        return d.min_hz;
    }

    int64_t span = (int64_t) d.max_hz - d.min_hz;
    int32_t hz   = d.min_hz + dfl_round_div((int64_t) (x - x0) * span, w);

    if (hz < d.min_hz) {
        hz = d.min_hz;
    }
    if (hz > d.max_hz) {
        hz = d.max_hz;
    }

    return hz;
}

/* --- BW -------------------------------------------------------------------
 * Espejo de src/cfg/mode.c::on_cur_filter_bw_change(): en AM/SAM/NFM/WFM el
 * BW crece desde LOW (que queda en 0); en el resto LOW/HIGH se recentran
 * alrededor del centro actual (division entera, truncando hacia cero). */
static inline void dfl_bw_targets(x6200_mode_t mode, int32_t low, int32_t high, int32_t bw,
                                  int32_t *new_low, int32_t *new_high) {
    if (dfl_mode_is_symmetric(mode)) {
        *new_low  = low;
        *new_high = low + bw;
        return;
    }

    *new_low  = (high + low - bw) / 2;
    *new_high = *new_low + bw;
}

/* Un BW es legal si el par LOW/HIGH resultante respeta los limites del modo:
 * HIGH <= MAX_FILTER_FREQ, HIGH > LOW y, en SSB, LOW no negativo. En CW/CWR el
 * LOW negativo es valido y en AM/SAM/NFM/WFM LOW queda como estaba (0). */
static inline bool dfl_bw_legal(x6200_mode_t mode, int32_t new_low, int32_t new_high) {
    if (new_high > MAX_FILTER_FREQ) {
        return false;
    }
    if (new_high <= new_low) {
        return false;
    }
    if (!dfl_mode_is_cw(mode) && !dfl_mode_is_symmetric(mode) && new_low < 0) {
        return false;
    }
    return true;
}

/* --- Restablecer predeterminado ------------------------------------------
 * Los setters reales tienen guardas: cfg_mode_set_high_filter() exige
 * val > cur_low y cfg_mode_set_low_filter() exige val < cur_high. Para escribir
 * el par (def_low, def_high) sin que una guarda rechace la segunda escritura,
 * el orden depende del estado actual: si def_high ya es mayor que cur_low se
 * puede subir HIGH primero; si no, hay que bajar LOW primero. Devuelve true
 * cuando LOW va primero. (def_low < def_high siempre, por tabla del firmware.) */
static inline bool dfl_reset_low_first(int32_t def_low, int32_t def_high, int32_t cur_low, int32_t cur_high) {
    (void) def_low;
    (void) cur_high;
    return !(def_high > cur_low);
}

/* --- Handles -------------------------------------------------------------- */

typedef enum {
    DFL_HANDLE_NONE = 0,
    DFL_HANDLE_LOW,
    DFL_HANDLE_HIGH,
} dfl_handle_t;

/* Hit-test de los handles del grafico (issue #179, D/C.2): un toque/arrastre
 * SOLO captura un handle si cae a `DFL_HANDLE_MAX_DIST` px o menos de su x (la
 * zona util de toque es >=44x44 px). Un toque lejano NO captura nada: devuelve
 * DFL_HANDLE_NONE y el filtro no se modifica. Es la correccion del bug del
 * #173/#177, donde dfl_nearest_handle() siempre devolvia un handle. */
#define DFL_HANDLE_MAX_DIST 22

static inline dfl_handle_t dfl_hit_test(int32_t x, int32_t low_x, int32_t high_x) {
    int32_t dl = x - low_x;
    int32_t dh = x - high_x;

    if (dl < 0) {
        dl = -dl;
    }
    if (dh < 0) {
        dh = -dh;
    }

    if (dl > DFL_HANDLE_MAX_DIST && dh > DFL_HANDLE_MAX_DIST) {
        return DFL_HANDLE_NONE;
    }

    return (dl <= dh) ? DFL_HANDLE_LOW : DFL_HANDLE_HIGH;
}

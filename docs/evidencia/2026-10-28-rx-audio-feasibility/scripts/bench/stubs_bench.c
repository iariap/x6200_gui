/* Stubs de enlace para compilar src/audio_enhancement.c VERBATIM en el banco.
 *
 * Solo se sustituye la periferia que en la radio proveen LVGL, cfg, params y
 * liquid-dsp. SpeexDSP NO se sustituye: se enlaza la libspeexdsp REAL del
 * contenedor. Las funciones de liquid abortan si alguien las llama, para que
 * ningun caso pueda "medir" un notch que no existe.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cfg/cfg.h"
#include "cfg/subjects.h"
#include "liquid/liquid.h"
#include "lvgl/lvgl.h"
#include "params/params.h"

params_t  params;
cfg_cur_t cfg_cur;

int32_t subject_get_int(Subject *subj) {
    (void) subj;

    return 0; /* modo distinto de CW/CWR: el banco nunca llama audio_enh_apply_mode_preset() */
}

uint32_t lv_tick_get(void) {
    /* Reloj monotonico simulado: ~5.8 ms por chunk de 256 muestras a 44.1 kHz. */
    static uint32_t tick_ms = 0;

    tick_ms += 6;

    return tick_ms;
}

/* --- liquid-dsp: NO se usa en ningun caso medido (0 notches activos) ---------- */

firfilt_rrrf firfilt_rrrf_create_notch(unsigned int m, float as, float f0) {
    (void) m;
    (void) as;
    (void) f0;
    fprintf(stderr, "FATAL: liquid-dsp no esta en el banco y se intento crear un notch\n");
    abort();
}

void firfilt_rrrf_destroy(firfilt_rrrf q) {
    (void) q;
    fprintf(stderr, "FATAL: liquid-dsp no esta en el banco y se intento destruir un notch\n");
    abort();
}

void firfilt_rrrf_push(firfilt_rrrf q, float x) {
    (void) q;
    (void) x;
    fprintf(stderr, "FATAL: liquid-dsp no esta en el banco y se intento empujar una muestra\n");
    abort();
}

void firfilt_rrrf_execute(firfilt_rrrf q, float *y) {
    (void) q;
    (void) y;
    fprintf(stderr, "FATAL: liquid-dsp no esta en el banco y se intento ejecutar un notch\n");
    abort();
}

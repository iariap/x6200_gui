/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  CB 27 MHz (Banda Ciudadana): planes, mapeo frecuencia <-> canal y ventana de
 *  TX (issue #146).
 *
 *  Logica pura: tablas y aritmetica de Hz, sin LVGL, sin subjects y sin SQLite.
 *  Se compila tal cual en tests/native/ (mismo patron que adaptive_tuning.c).
 *
 *  El plan de 40 canales es el CEPT (= FCC = Argentina): cinco huecos (26.995 /
 *  27.045 / 27.095 / 27.145 / 27.195) y los canales 23/24/25 fuera de orden. La
 *  MISMA secuencia de offsets en kHz vale para los cuatro grids, cada uno desde
 *  su canal 1 (seccion 4.1/4.2 del issue).
 */

#include "cb.h"

#include <stdio.h>

/* Base (frecuencia del canal 1) de cada grid, indexada por `plan - CB_PLAN_A`
 * (seccion 4.2). */
static const int32_t cb_plan_base[4] = {
    26065000,  /* CB A: 26,065-26,505 */
    26515000,  /* CB B: 26,515-26,955 */
    26965000,  /* CB C: 26,965-27,405 (CEPT/FCC/Argentina) */
    27415000,  /* CB D: 27,415-27,855 */
};

/* Offset de cada canal 1..40 respecto del canal 1 del grid, en kHz (seccion
 * 4.1). Los huecos (30 -> 60, 70 -> 90, 130 -> 140, 180 -> 190, 230 -> 240) y el
 * desorden 23/24/25 (290/270/280, o sea 27.255/27.235/27.245) son deliberados. */
static const int16_t cb_channel_offset_khz[40] = {
     0,  10,  20,  40,  50,  60,  70,  90, 100, 110,   /* ch 1..10  */
   120, 140, 150, 160, 170, 190, 200, 210, 220, 240,   /* ch 11..20 */
   250, 260, 290, 270, 280, 300, 310, 320, 330, 340,   /* ch 21..30 */
   350, 360, 370, 380, 390, 400, 410, 420, 430, 440,   /* ch 31..40 */
};

/* Region donde el interlock actua (seccion 7.2): fuera de 26-28 MHz no se toca
 * nada, las bandas de aficionado siguen igual. El extremo superior es EXCLUSIVO
 * para que 10 m (28.000) y todo lo de arriba quede afuera. */
#define CB_TX_REGION_MIN 26000000
#define CB_TX_REGION_MAX 28000000

/* SBC argentino: unica ventana que permite TX con cb_tx_scope = 0 (seccion 7.2).
 * El extremo alto de la Res. 47/82 cae en 27.410, un canal arriba del 27.405 del
 * plan CEPT. */
#define CB_TX_SBC_MIN 26960000
#define CB_TX_SBC_MAX 27410000

/* Base en Hz de un grid, o 0 si `plan` no es un grid valido (CB_PLAN_NONE
 * incluido). */
static int32_t cb_plan_base_hz(cb_plan_t plan) {
    if (plan < CB_PLAN_A || plan > CB_PLAN_D) {
        return 0;
    }
    return cb_plan_base[(int) plan - (int) CB_PLAN_A];
}

/* Offset en Hz del canal `ch` (1..40) dentro de su grid. `ch` tiene que venir ya
 * recortado a 1..40. */
static int32_t cb_channel_offset_hz(int ch) {
    return (int32_t) cb_channel_offset_khz[ch - 1] * 1000;
}

bool cb_plan_of_freq(int32_t freq, cb_plan_t *plan) {
    for (int i = 0; i < 4; i++) {
        int32_t base = cb_plan_base[i];
        /* Rango inclusivo [canal 1, canal 40] del grid. */
        if (freq >= base && freq <= base + cb_channel_offset_hz(CB_CHANNEL_MAX)) {
            if (plan != NULL) {
                *plan = (cb_plan_t) (i + (int) CB_PLAN_A);
            }
            return true;
        }
    }
    return false;
}

int cb_channel_of_freq(cb_plan_t plan, int32_t freq) {
    int32_t base = cb_plan_base_hz(plan);
    if (base == 0) {
        return CB_CHANNEL_NONE;
    }
    int32_t off = freq - base;
    if (off < 0 || off > cb_channel_offset_hz(CB_CHANNEL_MAX)) {
        return CB_CHANNEL_NONE;
    }
    /* 1..40 solo si la frecuencia es EXACTAMENTE un canal: los cinco huecos y
     * cualquier valor intermedio dan CB_CHANNEL_NONE aunque caigan en el rango. */
    for (int ch = CB_CHANNEL_MIN; ch <= CB_CHANNEL_MAX; ch++) {
        if (off == cb_channel_offset_hz(ch)) {
            return ch;
        }
    }
    return CB_CHANNEL_NONE;
}

int32_t cb_freq_of_channel(cb_plan_t plan, int channel) {
    int32_t base = cb_plan_base_hz(plan);
    if (base == 0) {
        return 0;
    }
    if (channel < CB_CHANNEL_MIN) {
        channel = CB_CHANNEL_MIN;
    } else if (channel > CB_CHANNEL_MAX) {
        channel = CB_CHANNEL_MAX;
    }
    return base + cb_channel_offset_hz(channel);
}

int cb_step_channel(cb_plan_t plan, int32_t freq, int delta) {
    int32_t base = cb_plan_base_hz(plan);
    if (base == 0) {
        return CB_CHANNEL_NONE;
    }
    if (delta == 0) {
        return cb_channel_of_freq(plan, freq);
    }

    int ch     = cb_channel_of_freq(plan, freq);
    int target;
    if (ch != CB_CHANNEL_NONE) {
        /* Canal exacto: el dial cuenta CANALES, no Hz. Por eso 23 -> 24 sube el
         * numero y BAJA la frecuencia (27.255 -> 27.235), sin sentirse al reves. */
        target = ch + delta;
    } else {
        /* Fuera de canal: engancha el PRIMER canal en la direccion del giro (no
         * el mas cercano) y desde ahi suma los detents que falten. */
        int32_t off = freq - base;
        if (delta > 0) {
            /* Primer canal por encima de `freq`; si no hay, se recorta a 40. */
            target = CB_CHANNEL_MAX;
            for (int i = 0; i < 40; i++) {
                if ((int32_t) cb_channel_offset_khz[i] * 1000 > off) {
                    target = i + 1;
                    break;
                }
            }
            target += delta - 1;  /* un detent se gasto en enganchar */
        } else {
            /* Primer canal por debajo de `freq`; si no hay, se recorta a 1. */
            target = CB_CHANNEL_MIN;
            for (int i = 39; i >= 0; i--) {
                if ((int32_t) cb_channel_offset_khz[i] * 1000 < off) {
                    target = i + 1;
                    break;
                }
            }
            target += delta + 1;
        }
    }
    /* Sin wrap: los extremos 1 y 40 no dan la vuelta. */
    if (target < CB_CHANNEL_MIN) {
        target = CB_CHANNEL_MIN;
    } else if (target > CB_CHANNEL_MAX) {
        target = CB_CHANNEL_MAX;
    }
    return target;
}

bool cb_tx_allowed(int32_t freq, int tx_scope) {
    if (tx_scope == CB_TX_SCOPE_OFF) {
        return true;  /* sin interlock: comportamiento historico */
    }
    if (freq < CB_TX_REGION_MIN || freq >= CB_TX_REGION_MAX) {
        return true;  /* fuera de 26-28 MHz no se toca nada (aficionados intactos) */
    }
    if (freq >= CB_TX_SBC_MIN && freq <= CB_TX_SBC_MAX) {
        return true;  /* SBC argentino, permitido en todas las politicas */
    }
    if (tx_scope == CB_TX_SCOPE_ALL_GRIDS) {
        /* Los rangos A, B y D (el C ya vive dentro del SBC). Ventana por
         * frecuencia, no por canal exacto: los huecos de un grid tambien pasan. */
        return cb_plan_of_freq(freq, NULL);
    }
    return false;
}

const char *cb_plan_name(cb_plan_t plan) {
    switch (plan) {
        case CB_PLAN_A: return "CB A";
        case CB_PLAN_B: return "CB B";
        case CB_PLAN_C: return "CB C";
        case CB_PLAN_D: return "CB D";
        default:        return "";   /* CB_PLAN_NONE y cualquier valor invalido */
    }
}

bool cb_channel_label(int32_t freq, char *buf, size_t buf_len) {
    cb_plan_t plan;
    if (!cb_plan_of_freq(freq, &plan)) {
        return false;  /* no es CB: la celda conserva su rotulo de siempre */
    }
    if (buf == NULL || buf_len == 0) {
        return false;
    }
    int ch = cb_channel_of_freq(plan, freq);
    if (ch == CB_CHANNEL_NONE) {
        snprintf(buf, buf_len, "CH --");
    } else {
        snprintf(buf, buf_len, "CH %d", ch);
    }
    return true;
}

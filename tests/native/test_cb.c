/*
 * Unitarias del modulo CB 27 MHz (src/cb.c, issue #146).
 *
 * El modulo es puro -tablas y aritmetica de Hz, sin LVGL, sin subjects, sin
 * SQLite-, asi que corre tal cual en el host con los sanitizers (mismo patron
 * que adaptive_tuning.c). Los casos son los de la seccion 10.1 del issue, mas
 * los bordes que sostienen las dos promesas que el codigo no puede dejar
 * libradas a un comentario:
 *
 *   - la ventana de TX: los extremos del SBC argentino (26.960 / 27.410), los
 *     de la region donde el interlock actua (26-28 MHz, ver cb_tx_allowed) y
 *     los de los grids A/B/D con cb_tx_scope = 1;
 *   - el plan: los extremos 1/40 de los cuatro grids, los cinco huecos y el
 *     desorden 23/24/25.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "cb.h"

#include <string.h>

#include "test_framework.h"

void test_cb(void) {
    cb_plan_t plan = CB_PLAN_NONE;
    char      label[8];

    /* --- cb_plan_of_freq: rangos inclusivos de los cuatro grids (4.2) ------- */

    /* 26.064.999 no es CB: el grid A arranca en 26.065 */
    CHECK(!cb_plan_of_freq(26064999, &plan));

    CHECK(cb_plan_of_freq(26065000, &plan) && plan == CB_PLAN_A);
    CHECK(cb_plan_of_freq(26505000, &plan) && plan == CB_PLAN_A);

    CHECK(cb_plan_of_freq(26515000, &plan) && plan == CB_PLAN_B);
    CHECK(cb_plan_of_freq(26955000, &plan) && plan == CB_PLAN_B);

    CHECK(cb_plan_of_freq(26965000, &plan) && plan == CB_PLAN_C);
    CHECK(cb_plan_of_freq(27405000, &plan) && plan == CB_PLAN_C);

    CHECK(cb_plan_of_freq(27415000, &plan) && plan == CB_PLAN_D);
    CHECK(cb_plan_of_freq(27855000, &plan) && plan == CB_PLAN_D);

    /* 27.4055 (el hueco entre C y D), 27.410 y 27.856 no son CB */
    CHECK(!cb_plan_of_freq(27405500, &plan));
    CHECK(!cb_plan_of_freq(27410000, &plan));
    CHECK(!cb_plan_of_freq(27856000, &plan));

    /* Fuera de 26-28 MHz ni se mira */
    CHECK(!cb_plan_of_freq(14074000, &plan));
    CHECK(!cb_plan_of_freq(28074000, &plan));

    /* El puntero de salida es opcional: sirve para preguntar "es CB?" a secas */
    CHECK(cb_plan_of_freq(27185000, NULL));

    /* --- cb_channel_of_freq: 1..40 solo con el canal EXACTO (5.1) ----------- */

    CHECK(cb_channel_of_freq(CB_PLAN_C, 27185000) == 19);
    CHECK(cb_channel_of_freq(CB_PLAN_C, 27255000) == 23);
    CHECK(cb_channel_of_freq(CB_PLAN_C, 27235000) == 24);
    CHECK(cb_channel_of_freq(CB_PLAN_C, 27245000) == 25);
    CHECK(cb_channel_of_freq(CB_PLAN_C, 26965000) == 1);
    CHECK(cb_channel_of_freq(CB_PLAN_C, 27405000) == 40);

    /* 27.195 es uno de los cinco huecos (26.995 / 27.045 / 27.095 / 27.145 /
     * 27.195): esta dentro del grid, pero no es canal */
    CHECK(cb_channel_of_freq(CB_PLAN_C, 27195000) == 0);
    /* ...y tampoco lo es cualquier frecuencia intermedia */
    CHECK(cb_channel_of_freq(CB_PLAN_C, 27185500) == 0);
    CHECK(cb_channel_of_freq(CB_PLAN_C, 27000000) == 0);

    CHECK(cb_channel_of_freq(CB_PLAN_A, 26325000) == 22);
    CHECK(cb_channel_of_freq(CB_PLAN_A, 26065000) == 1);

    /* --- cb_freq_of_channel: base del grid + offset en kHz (4.1) ----------- */

    CHECK(cb_freq_of_channel(CB_PLAN_C, 1) == 26965000);
    CHECK(cb_freq_of_channel(CB_PLAN_C, 40) == 27405000);

    CHECK(cb_freq_of_channel(CB_PLAN_A, 1) == 26065000);
    CHECK(cb_freq_of_channel(CB_PLAN_A, 40) == 26505000);
    CHECK(cb_freq_of_channel(CB_PLAN_B, 1) == 26515000);
    CHECK(cb_freq_of_channel(CB_PLAN_D, 1) == 27415000);
    CHECK(cb_freq_of_channel(CB_PLAN_D, 40) == 27855000);

    /* 23/24/25 salen fuera de orden (26.3 del manual) */
    CHECK(cb_freq_of_channel(CB_PLAN_C, 23) == 27255000);
    CHECK(cb_freq_of_channel(CB_PLAN_C, 24) == 27235000);
    CHECK(cb_freq_of_channel(CB_PLAN_C, 25) == 27245000);

    /* Un canal fuera de 1..40 se recorta, no devuelve cualquier cosa */
    CHECK(cb_freq_of_channel(CB_PLAN_C, 0) == 26965000);
    CHECK(cb_freq_of_channel(CB_PLAN_C, 41) == 27405000);

    /* --- cb_step_channel: 1 detent = 1 canal, sin wrap (5.1) -------------- */

    CHECK(cb_step_channel(CB_PLAN_C, 26965000, -1) == 1);      /* recorta abajo */
    CHECK(cb_step_channel(CB_PLAN_C, 27405000, +1) == 40);     /* recorta arriba */
    CHECK(cb_step_channel(CB_PLAN_A, 26065000, -1) == 1);
    CHECK(cb_step_channel(CB_PLAN_D, 27855000, +1) == 40);

    CHECK(cb_step_channel(CB_PLAN_C, 27225000, +1) == 23);
    /* El 23 -> 24 sube el NUMERO de canal y baja la frecuencia: el dial cuenta
     * canales, no Hz (5.1 / 6.3) */
    CHECK(cb_step_channel(CB_PLAN_C, 27255000, +1) == 24);
    CHECK(cb_step_channel(CB_PLAN_C, 27235000, +2) == 26);

    /* Fuera de canal: engancha el primer canal en la direccion del giro (no el
     * mas cercano), y desde ahi los detents que falten */
    CHECK(cb_step_channel(CB_PLAN_C, 27000000, +1) == 4);      /* 27.005 */
    CHECK(cb_step_channel(CB_PLAN_C, 27000000, -1) == 3);      /* 26.985 */
    CHECK(cb_step_channel(CB_PLAN_C, 26997000, +1) == 4);      /* hueco 26.995 */
    CHECK(cb_step_channel(CB_PLAN_C, 26997000, -1) == 3);
    CHECK(cb_step_channel(CB_PLAN_C, 27195000, +1) == 20);     /* hueco 27.195 */
    CHECK(cb_step_channel(CB_PLAN_C, 27195000, -1) == 19);
    CHECK(cb_step_channel(CB_PLAN_A, 26000000, +1) == 1);      /* debajo del grid */
    CHECK(cb_step_channel(CB_PLAN_D, 28000000, -1) == 40);     /* arriba del grid */

    /* --- cb_tx_allowed: ventana de TX por politica (7.2) ------------------ */

    /* cb_tx_scope = 0: solo el SBC argentino, 26.960-27.410 */
    CHECK(cb_tx_allowed(27185000, 0));
    CHECK(cb_tx_allowed(26960000, 0));                         /* extremo bajo */
    CHECK(cb_tx_allowed(27410000, 0));                         /* extremo alto */
    CHECK(!cb_tx_allowed(26959999, 0));
    CHECK(!cb_tx_allowed(27410001, 0));
    CHECK(!cb_tx_allowed(26515000, 0));                        /* grid B: RX */
    CHECK(!cb_tx_allowed(26065000, 0));                        /* grid A: RX */
    CHECK(!cb_tx_allowed(27415000, 0));                        /* grid D: RX */
    CHECK(!cb_tx_allowed(27410000 + 500, 0));

    /* cb_tx_scope = 1: ademas los grids A, B y D (el C ya esta en el SBC) */
    CHECK(cb_tx_allowed(26515000, 1));
    CHECK(cb_tx_allowed(26065000, 1));
    CHECK(cb_tx_allowed(26505000, 1));
    CHECK(cb_tx_allowed(26955000, 1));
    CHECK(cb_tx_allowed(27415000, 1));
    CHECK(cb_tx_allowed(27855000, 1));
    CHECK(cb_tx_allowed(27185000, 1));
    CHECK(!cb_tx_allowed(26510000, 1));                        /* hueco A-B: no */
    CHECK(!cb_tx_allowed(27856000, 1));                        /* arriba de D: no */

    /* cb_tx_scope = 2: sin interlock */
    CHECK(cb_tx_allowed(26515000, 2));
    CHECK(cb_tx_allowed(27185000, 2));
    CHECK(cb_tx_allowed(28074000, 2));

    /* Fuera de la region 26-28 MHz no se toca nada: el interlock solo actua
     * adentro (10 m arranca en 28.000 y queda afuera) */
    CHECK(cb_tx_allowed(28074000, 0));
    CHECK(cb_tx_allowed(28000000, 0));
    CHECK(!cb_tx_allowed(27999999, 0));
    CHECK(cb_tx_allowed(25999999, 0));
    CHECK(cb_tx_allowed(14074000, 0));
    CHECK(cb_tx_allowed(0, 0));

    /* --- cb_channel_label: la celda VFO de la barra (6.2) ------------------ */

    CHECK(cb_channel_label(27185000, label, sizeof(label)) && !strcmp(label, "CH 19"));
    CHECK(cb_channel_label(27000000, label, sizeof(label)) && !strcmp(label, "CH --"));
    CHECK(cb_channel_label(27195000, label, sizeof(label)) && !strcmp(label, "CH --"));
    CHECK(!cb_channel_label(14074000, label, sizeof(label)));
    CHECK(!cb_channel_label(27185000, NULL, 0));
}

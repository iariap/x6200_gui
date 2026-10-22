/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Politica del squelch de voz (issue #166). Ver src/sql_policy.h.
 */

#include "sql_policy.h"

bool sql_should_enable(uint8_t level) {
    return level > 0;
}

void sql_policy_apply(x6200_mode_t mode, uint8_t level) {
    switch (mode) {
        case x6200_mode_nfm:
        case x6200_mode_wfm:
            x6200_control_sql_fm_set(level);
            break;

        default:
            x6200_control_sql_set(level);
            break;
    }

    /* El enable se escribe DESPUES del nivel: el nivel gobierna el umbral y el
     * flag lo habilita. Idempotente con la lib actual (que ya lo acopla dentro
     * de sql_set/sql_fm_set), pero deja el contrato explicito en el camino de
     * radio y cubre el arranque. */
    x6200_control_sql_enable_set(sql_should_enable(level));
}

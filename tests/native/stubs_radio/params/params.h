/*
 * Minimal params stub for the radio_thread test (issue #219).
 *
 * Only the fields src/radio.c reads/writes are declared, with the shapes of
 * src/params/types.h. The radio module must compile without pulling the real
 * params/params.h (which drags clock.h, voice.h, cfg.h, SQLite and lvgl).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <aether_radio/x6200_control/control.h>

#include "../radio.h"
#include "../clock.h"
#include "../voice.h"

typedef struct { char *name; char *voice; bool    x; bool dirty; } params_bool_t;
typedef struct { char *name; char *voice; float   x; bool dirty; } params_float_t;
typedef struct { char *name; char *voice; uint8_t x; uint8_t min; uint8_t max; bool dirty; } params_uint8_t;

typedef struct {
    bool                vol_modes;
    bool                mfk_modes;
    int16_t             brightness_normal;

    /* Perfil visual de bateria (UI-08 incr. 5): override manual o automatico. */
    params_uint8_t      visual_profile;

    /* radio */
    x6200_mic_sel_t     mic;
    uint8_t             hmic;
    uint8_t             imic;
    radio_charger_t     charger;
    uint16_t            bias_drive;
    uint16_t            bias_final;
    int16_t             rit;
    int16_t             xit;
    uint8_t             line_in;
    uint8_t             line_out;
    int16_t             moni;
    params_bool_t       spmode;

    /* CB 27 MHz (issue #146): politica y tope de potencia. */
    params_uint8_t      cb_tx_scope;
    params_float_t      cb_pwr_max;

    /* VOX */
    bool                vox;
    uint8_t             vox_ag;
    uint16_t            vox_delay;
    uint8_t             vox_gain;

    /* main screen */
    params_bool_t       waterfall_battery;

    struct {
        bool    mic;
        bool    hmic;
        bool    imic;
        bool    charger;
        bool    rit;
        bool    xit;
        bool    line_in;
        bool    line_out;
        bool    moni;
    } dirty;
} params_t;

extern params_t params;

void params_lock(void);
void params_unlock(bool *dirty);
void params_bool_set(params_bool_t *var, bool x);

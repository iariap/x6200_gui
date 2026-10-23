/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MFK_MIN_LEVEL = 0,
    MFK_MAX_LEVEL,
    MFK_FFT_DECIM,
    MFK_SPECTRUM_BETA,
    MFK_PEAK_HOLD,
    MFK_PEAK_SPEED,

    MFK_SPECTRUM_FILL,
    MFK_SPECTRUM_PEAK,
    MFK_CHARGER,

    MFK_KEY_SPEED,
    MFK_KEY_MODE,
    MFK_IAMBIC_MODE,
    MFK_KEY_TONE,
    MFK_KEY_VOL,
    MFK_KEY_TRAIN,
    MFK_QSK_TIME,
    MFK_KEY_RATIO,

    MFK_DNF,
    MFK_DNF_CENTER,
    MFK_DNF_WIDTH,
    MFK_NB,
    MFK_NB_LEVEL,
    MFK_NB_WIDTH,
    MFK_NR,
    MFK_NR_LEVEL,

    MFK_AGC,
    MFK_AGC_HANG,
    MFK_AGC_KNEE,
    MFK_AGC_SLOPE,

    MFK_CW_DECODER,
    MFK_CW_TUNE,
    MFK_CW_DECODER_SNR,
    MFK_CW_DECODER_PEAK_BETA,
    MFK_CW_DECODER_NOISE_BETA,

    MFK_ANT,
    MFK_RIT,
    MFK_XIT,

    MFK_ATT,
    MFK_PRE,
    MFK_COMP,
    /* Append-only: los ids son bits de params.mfk_modes y se persisten. */
    MFK_CW_DECODER_AUTO,
    /* CW zoom viene del upstream v0.1.2, que lo inserto a media lista (entre
     * MFK_KEY_RATIO y MFK_DNF). Aca va al final a proposito: insertarlo en el
     * medio corre en uno todos los ids siguientes y reinterpreta el bitmask
     * `params.mfk_modes` que ya esta guardado en params.db (el usuario perderia
     * sus MFK: un DNF elegido pasaria a ser otra tecla). Misma regla que el
     * comentario de arriba. */
    MFK_CW_ZOOM,

    MFK_LAST,

    /* APPs */

    MFK_RTTY_RATE,
    MFK_RTTY_SHIFT,
    MFK_RTTY_CENTER,
    MFK_RTTY_REVERSE,
} mfk_mode_t;

typedef enum {
    MFK_STATE_EDIT = 0,
    MFK_STATE_SELECT
} mfk_state_t;

extern mfk_state_t  mfk_state;

void mfk_update(int16_t diff, bool voice);

/* The X6100 puts the encoder into EDIT mode on one of the app buttons
 * (WeFax align/tilt). The control registry is not ported yet, so the call is
 * accepted and ignored: the values are still adjustable through params. */
void mfk_set_ctrl(int ctrl);
void mfk_change_mode(int16_t dir);
void mfk_set_mode(mfk_mode_t mode);

/* The mode the encoder is on (the apps draw their own knob tags) */
mfk_mode_t mfk_get_mode(void);

#ifdef __cplusplus
}
#endif

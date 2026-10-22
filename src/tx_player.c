/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - digital-mode TX player
 *
 *  Copyright (c) 2026
 */

#include "tx_player.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "lvgl/lvgl.h"

#include "audio.h"
#include "cfg/cfg_api.h"
#include "params/params.h"
#include "radio.h"
#include "tx_info.h"
#include "tx_level.h"

#include <aether_radio/x6200_control/control.h>

#define GAIN_MIN_DB (-30.0f)
#define GAIN_MAX_DB 0.0f

/* The learned offset (kept between transmissions) never below this: an overdriven or long
 * transmission (SSTV) could push it to the floor, and the ramp up (8 dB a transmission at
 * most, tx_level.c) took many transmissions to bring the power back */
#define LEARNED_MIN_DB (-12.0f)

/* The X6100 read the OEM base revision from its control library; ours only
 * exposes the version string. Which drive behaviour the base has can only be
 * confirmed on the radio, so use the conservative r3 path (the drive follows
 * the power reading) until then. */
static int tx_player_base_rev(void) {
    return 3;
}

static float clamp_gain(float g) {
    return g > GAIN_MAX_DB ? GAIN_MAX_DB : g < GAIN_MIN_DB ? GAIN_MIN_DB : g;
}

float tx_player_base_gain_offset(void) {
    float target_pwr = LV_MIN(param_f_get(cfg_pwr), TX_PLAYER_MAX_PWR_W);
    if (tx_player_base_rev() >= 3) {
        // patched firmware has a true power control
        return -9.4f;
    }
    return -16.4f + log10f(target_pwr) * 10.0f;
}

typedef struct {
    const int16_t *ptr;
    uint32_t       left;
} buf_src_t;

static size_t buf_fill(int16_t *buf, size_t n, void *ctx) {
    buf_src_t *src = ctx;

    n = LV_MIN(n, src->left);
    memcpy(buf, src->ptr, n * sizeof(*buf));
    src->ptr  += n;
    src->left -= n;
    return n;
}

bool tx_player_play(int16_t      *samples,
                    uint32_t      n_samples,
                    int32_t       tx_offset_hz,
                    float         base_gain_offset,
                    tx_abort_fn_t abort_check,
                    void         *abort_check_ctx) {
    buf_src_t src = {samples, n_samples};

    return tx_player_stream(buf_fill, &src, tx_offset_hz, base_gain_offset, abort_check, abort_check_ctx);
}

bool tx_player_stream(tx_fill_fn_t  fill,
                      void         *fill_ctx,
                      int32_t       tx_offset_hz,
                      float         base_gain_offset,
                      tx_abort_fn_t abort_check,
                      void         *abort_check_ctx) {
    bool capped = param_f_get(cfg_pwr) > TX_PLAYER_MAX_PWR_W;
    if (capped) {
        radio_set_pwr(TX_PLAYER_MAX_PWR_W);
    }

    float gain_offset      = base_gain_offset + LV_MAX(params.ft8_output_gain_offset.x, LEARNED_MIN_DB);
    float play_gain_offset = audio_set_play_vol(gain_offset + 6.0f);
    gain_offset           -= play_gain_offset;

    uint64_t radio_freq = cparam_i_get(cfg_fg_freq);
    radio_set_freq((int32_t)radio_freq + tx_offset_hz - TX_PLAYER_AUDIO_HZ);
    radio_set_modem(true);

    /* The drive from the ALC and power readback (tx_level.c): ramped up while the ALC reads
     * zero, held once it shows, kept for the next transmission */
    static uint8_t msg_id = 0;
    tx_level_t     level;
    tx_level_start(&level, LV_MIN(param_f_get(cfg_pwr), TX_PLAYER_MAX_PWR_W));
    tx_info_refresh(&msg_id, NULL, NULL, NULL); /* readings from before this one don't count */

    float    prev_gain_offset = gain_offset;
    int16_t  ptr[1024 * 2];
    size_t   part;

    /* Baseband r13: the power is held at the setting by the base, whose ALC then reads 8-10 at any
     * drive (measured from -12 to 0 dB, 0.5 and 5 W): taken as overdrive it brought the level down
     * to the floor at every transmission. There the level follows the power reading only.
     * ponytail: measured on r13; r3-r12 keep the ALC until checked on one */
    bool use_alc = tx_player_base_rev() < 13;

    bool aborted = false;
    while (true) {
        float pwr = 0.0f, alc = 0.0f;
        bool  have = tx_info_refresh(&msg_id, &alc, &pwr, NULL);
        if (!use_alc) alc = 0.0f;
        gain_offset = clamp_gain(gain_offset + tx_level_block(&level, have, pwr, alc));
        part = fill(ptr, sizeof(ptr) / sizeof(ptr[0]), fill_ctx);
        if (part == 0) {
            break;
        }
        if (abort_check && abort_check(abort_check_ctx)) {
            aborted = true;
            break;
        }
        if (gain_offset == prev_gain_offset) {
            if (gain_offset != 0.0f) {
                audio_gain_db(ptr, part, gain_offset, ptr);
            }
        } else {
            audio_gain_db_transition(ptr, part, prev_gain_offset, gain_offset, ptr);
            prev_gain_offset = gain_offset;
        }
        audio_play(ptr, part);
    }

    /* The learned gain offset is shared by FT8 and JS8: same audio path. */
    params_float_set(&params.ft8_output_gain_offset,
                     LV_MAX(gain_offset - base_gain_offset + play_gain_offset, LEARNED_MIN_DB));
    audio_play_wait();
    radio_set_modem(false);
    radio_set_freq((int32_t)radio_freq);
    audio_set_play_vol(params.play_gain_db_f.x);
    if (capped) {
        radio_set_pwr(param_f_get(cfg_pwr));   /* RTTY and WeFax left the radio at the cap, also in SSB after */
    }

    return !aborted;
}

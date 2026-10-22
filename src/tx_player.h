/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - digital-mode TX player
 *
 *  Plays a prepared TX waveform with the radio keyed, correcting the audio
 *  gain from the radio's ALC and power readback as it goes. Shared by the
 *  FT8 and JS8 apps; moved here unchanged from src/ft8/tx_worker.c.
 *
 *  The waveform is synthesised around TX_PLAYER_AUDIO_HZ; while it plays
 *  the VFO is shifted so the signal lands at the requested audio offset,
 *  and restored afterwards. Blocks for the length of the waveform, so call
 *  it from a worker thread.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Audio tone the waveform is centred on. */
#define TX_PLAYER_AUDIO_HZ 1325

/* Digital modes are capped at this power. */
#define TX_PLAYER_MAX_PWR_W 7.0f

/* Abort-check callback. Return true to stop TX after the current block. */
typedef bool (*tx_abort_fn_t)(void *ctx);

/* Starting gain offset (dB) for the radio's firmware and power setting. */
float tx_player_base_gain_offset(void);

/* Fill callback for tx_player_stream: write up to n samples into buf and
 * return how many; 0 ends the transmission. */
typedef size_t (*tx_fill_fn_t)(int16_t *buf, size_t n, void *ctx);

/* Key the radio and play n_samples of int16 audio at AUDIO_PLAY_RATE,
 * shifted to tx_offset_hz. The samples are left unchanged. Returns false if
 * abort_check stopped it early. */
bool tx_player_play(int16_t      *samples,
                    uint32_t      n_samples,
                    int32_t       tx_offset_hz,
                    float         base_gain_offset,
                    tx_abort_fn_t abort_check,
                    void         *abort_check_ctx);

/* As tx_player_play, with the audio generated block by block by fill (for
 * waveforms too long to hold in memory, e.g. WeFax). */
bool tx_player_stream(tx_fill_fn_t  fill,
                      void         *fill_ctx,
                      int32_t       tx_offset_hz,
                      float         base_gain_offset,
                      tx_abort_fn_t abort_check,
                      void         *abort_check_ctx);

#ifdef __cplusplus
}
#endif

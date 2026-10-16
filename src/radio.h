/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>
#include <aether_radio/x6200_control/control.h>

#include "lvgl/lvgl.h"

#define RADIO_SAMPLES   (512)

typedef enum {
    RADIO_RX = 0,
    RADIO_TX,
    RADIO_ATU_START,
    RADIO_ATU_WAIT,
    RADIO_ATU_RUN,
    RADIO_SWRSCAN,

    RADIO_POWEROFF,
    RADIO_OFF
} radio_state_t;

typedef enum {
    RADIO_CHARGER_OFF = 0,
    RADIO_CHARGER_ON,
    RADIO_CHARGER_SHADOW
} radio_charger_t;

typedef void (*radio_state_change_t) ();

void radio_init(radio_state_change_t tx_cb, radio_state_change_t rx_cb);
void radio_bb_reset();
bool radio_tick();
radio_state_t radio_get_state();

/**
 * Set freq for radio without updating corresponding subject.
 * Useful for FT8 TX freq change and SWR scan
 */
void radio_set_freq(int32_t freq);
bool radio_check_freq(int32_t freq);

x6200_vfo_t radio_toggle_vfo();

uint16_t radio_change_vol(int16_t df);
uint16_t radio_change_moni(int16_t df);
bool radio_change_spmode(int16_t df);

void radio_change_mute();
bool radio_is_muted();

void radio_set_pwr(float d);

radio_charger_t radio_change_charger(int16_t d);

x6200_mic_sel_t radio_change_mic(int16_t d);
uint8_t radio_change_hmic(int16_t d);
uint8_t radio_change_imic(int16_t d);

void radio_start_atu();

bool radio_start_swrscan();
void radio_stop_swrscan();

/* Snapshot del estado del equipo (dial, modo, agc/pre/att y potencia) para las apps que lo mutan
 * al abrirse: guardarlo al abrir y devolverlo al cerrar (issue #94). Lo llama el marco de
 * dialogos; no toca la senal de RF, solo el estado. */
void radio_state_save();
void radio_state_restore();

void radio_poweroff();
void radio_set_ptt(bool tx);
void radio_set_modem(bool tx);

/* The Settings "Charger" row: only On charges while running; Shadow charges when
 * the radio is switched off (see radio_poweroff) */
void radio_set_charger(radio_charger_t v);

/* The X6200 control library has no TX filter or RX DSP on/off command (the
 * X6100 one did). The JS8 app asks for both; they are accepted and ignored so
 * the app works with the radio's own filter settings. */
void radio_set_tx_filter(uint16_t low, uint16_t high);
void radio_set_rx_dsp_off(bool off);

/* The app asks for the speaker to follow the transmission; this GUI drives the
 * playback volume itself, so the call is accepted and ignored. */
void radio_speaker_play(bool on);


int16_t radio_change_xit(int16_t d);
int16_t radio_change_rit(int16_t d);

void radio_set_line_in(uint8_t d);
void radio_set_line_out(uint8_t d);

void radio_set_morse_key(bool on);

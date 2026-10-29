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

#include "rx_audio_mux_guard.h"

#define RADIO_SAMPLES   (512)

/* Rango del nivel de audio de recepcion (registro 14 del BASE). El techo es 100 porque es el que
 * ya usan la perilla (src/vol.c), el control remoto (`vol` 0..100 en src/remote_server.cpp) y el
 * propio registro; el CAT de esta GUI mapea 0..55 (src/cat.cpp), asi que el rango real del equipo
 * queda como hipotesis en el anexo A de docs/HALLAZGOS.md (#244). */
#define VOL_MIN         (0)
#define VOL_MAX         (100)

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

/* Manda al BASE el nivel de audio de recepcion que corresponde al estado actual (mute del
 * usuario, mute por auriculares A2DP y cfg.vol.val). Es el unico camino que escribe ese
 * registro; la llama la perilla, el boton de mute, la politica de BT y el re-sync al salir de
 * CW (issue #244). */
void radio_vol_apply(void);

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

/* A-prime (#367): coordinador unico del bit voice_rec (registro 12 compartido con
 * PTT/ATU/modem). Falla cerrado: devuelve RXMUX_BLOCKED_NO_READBACK mientras no exista
 * una lectura real del MCU (Fase R). NUNCA escribe si el registro real tiene bits de
 * TX/ATU/modem/barrido/apagado, ni si otro camino (TTS/mensaje de voz/arranque) es el
 * dueno del bit. El router de #364 solo podra usarla cuando #363 vuelva a A_PASS. */
rxmux_result_t radio_audio_mux_request(bool dac);

/* Solo para tests/instrumentacion: registrar un backend de bus con readback real.
 * En produccion NO se registra (queda NULL) y todo request devuelve
 * RXMUX_BLOCKED_NO_READBACK. No expone I2C crudo: el backend lo implementa quien ya
 * es dueno del bus (la GUI). */
void radio_audio_mux_set_bus_ops(const rxmux_bus_ops_t *ops);

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

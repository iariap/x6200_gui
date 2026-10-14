/*
 *  Explicit radio state for the host UI harness (see mock_radio.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "mock_radio.h"

#include "cfg/cfg.h"
#include "cfg/subjects.h"

mock_radio_state_t mock_radio;

void mock_radio_init(void)
{
    mock_radio = (mock_radio_state_t){
        .frequency_hz    = 14074000,        /* 20 m, USB, the fixture's band */
        .tx_frequency_hz = 14074000,
        .fft_width_hz    = 384000,
        .mode            = x6200_mode_usb,
        .vfo             = X6200_VFO_A,
        .split           = false,
        .agc             = x6200_agc_auto,
        .att             = false,
        .pre             = false,
        .atu_enabled     = false,
        .ant             = 1,
        .smeter_db       = -73,             /* S9 */
        .battery_v       = 12.1f,
        .external_v      = 13.8f,
        .battery_cap     = 76,
        .charging        = false,
        .state           = RADIO_RX,
    };
}

void mock_radio_apply(void)
{
    subject_set_int(cfg_cur.fg_freq, (int32_t) mock_radio.frequency_hz);
    subject_set_int(cfg_cur.bg_freq, (int32_t) mock_radio.tx_frequency_hz);
    subject_set_int(cfg_cur.fft_width, (int32_t) mock_radio.fft_width_hz);
    subject_set_int(cfg_cur.mode, mock_radio.mode);
    subject_set_int(cfg_cur.agc, mock_radio.agc);
    subject_set_int(cfg_cur.att, mock_radio.att);
    subject_set_int(cfg_cur.pre, mock_radio.pre);
    subject_set_int(cfg_cur.band->vfo.val, mock_radio.vfo);
    subject_set_int(cfg_cur.band->split.val, mock_radio.split);
    subject_set_int(cfg_cur.atu->loaded, mock_radio.atu_enabled);
    subject_set_int(cfg.atu_enabled.val, mock_radio.atu_enabled);
    subject_set_int(cfg.ant_id.val, mock_radio.ant);
}

/* --- radio.h surface -------------------------------------------------------
 * Only the state machine is honoured; every command is a no-op that never
 * reaches a device. */

void radio_init(radio_state_change_t tx_cb, radio_state_change_t rx_cb)
{
    (void) tx_cb;
    (void) rx_cb;
}

void radio_bb_reset(void) {}
bool radio_tick(void) { return false; }

radio_state_t radio_get_state(void)
{
    return mock_radio.state;
}

void radio_set_freq(int32_t freq)
{
    mock_radio.frequency_hz = (uint32_t) freq;
}

bool radio_check_freq(int32_t freq)
{
    return (uint32_t) freq == mock_radio.frequency_hz;
}

x6200_vfo_t radio_toggle_vfo(void)
{
    mock_radio.vfo = mock_radio.vfo == X6200_VFO_A ? X6200_VFO_B : X6200_VFO_A;
    return mock_radio.vfo;
}

uint16_t radio_change_vol(int16_t df) { (void) df; return 0; }
uint16_t radio_change_moni(int16_t df) { (void) df; return 0; }
bool radio_change_spmode(int16_t df) { (void) df; return false; }

void radio_change_mute(void) {}
bool radio_is_muted(void) { return false; }

void radio_set_pwr(float d) { (void) d; }

radio_charger_t radio_change_charger(int16_t d) { (void) d; return RADIO_CHARGER_OFF; }
x6200_mic_sel_t radio_change_mic(int16_t d) { (void) d; return x6200_mic_builtin; }
uint8_t radio_change_hmic(int16_t d) { (void) d; return 0; }
uint8_t radio_change_imic(int16_t d) { (void) d; return 0; }

void radio_start_atu(void) {}
bool radio_start_swrscan(void) { return false; }
void radio_stop_swrscan(void) {}

void radio_poweroff(void) {}
void radio_set_ptt(bool tx) { (void) tx; }
void radio_set_modem(bool tx) { (void) tx; }
void radio_set_charger(radio_charger_t v) { (void) v; }
void radio_set_tx_filter(uint16_t low, uint16_t high) { (void) low; (void) high; }
void radio_set_rx_dsp_off(bool off) { (void) off; }
void radio_speaker_play(bool on) { (void) on; }

int16_t radio_change_xit(int16_t d) { (void) d; return 0; }
int16_t radio_change_rit(int16_t d) { (void) d; return 0; }

void radio_set_line_in(uint8_t d) { (void) d; }
void radio_set_line_out(uint8_t d) { (void) d; }
void radio_set_morse_key(bool on) { (void) on; }

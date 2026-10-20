/*
 *  Explicit radio state for the host UI harness (see mock_radio.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "mock_radio.h"

#include <string.h>

#include "cfg/cfg.h"
#include "cfg/subjects.h"

mock_radio_state_t mock_radio;
mock_radio_calls_t mock_radio_calls;

void mock_radio_calls_reset(void)
{
    memset(&mock_radio_calls, 0, sizeof(mock_radio_calls));
}

void mock_radio_init(void)
{
    mock_radio_calls_reset();
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
        .filter_set      = false,
        .filter_low      = 0,
        .filter_high     = 3000,
    };
}

/* Mapeo por modo de cfg_cur.filter.real.from/to, copia fiel de
 * src/cfg/mode.c::update_real_filters(): el fixture declara low/high del modo y
 * el arnes publica los mismos bordes reales de RF que la radio. */
static void apply_real_filter(void)
{
    int32_t from, to;

    switch (mock_radio.mode) {
        case x6200_mode_lsb:
        case x6200_mode_lsb_dig:
            from = -mock_radio.filter_high;
            to   = -mock_radio.filter_low;
            break;

        case x6200_mode_usb:
        case x6200_mode_usb_dig:
        case x6200_mode_cw:
        case x6200_mode_cwr:
            from = mock_radio.filter_low;
            to   = mock_radio.filter_high;
            break;

        case x6200_mode_am:
        case x6200_mode_sam:
        case x6200_mode_nfm:
        case x6200_mode_wfm:
            from = -mock_radio.filter_high;
            to   = mock_radio.filter_high;
            break;

        default:
            from = mock_radio.filter_low;
            to   = mock_radio.filter_high;
            break;
    }

    subject_set_int(cfg_cur.filter.low, mock_radio.filter_low);
    subject_set_int(cfg_cur.filter.high, mock_radio.filter_high);
    subject_set_int(cfg_cur.filter.bw, mock_radio.filter_high - mock_radio.filter_low);
    subject_set_int(cfg_cur.filter.real.from, from);
    subject_set_int(cfg_cur.filter.real.to, to);
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

    /* Solo cuando el fixture declara el filtro: los fixtures historicos conservan
     * la banda 0..3000 con la que se calibraron sus capturas. */
    if (mock_radio.filter_set) {
        apply_real_filter();
    }
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
    mock_radio_calls.get_state++;
    return mock_radio.state;
}

void radio_set_freq(int32_t freq)
{
    mock_radio_calls.set_freq++;
    mock_radio.frequency_hz = (uint32_t) freq;
}

bool radio_check_freq(int32_t freq)
{
    mock_radio_calls.check_freq++;
    return (uint32_t) freq == mock_radio.frequency_hz;
}

x6200_vfo_t radio_toggle_vfo(void)
{
    mock_radio_calls.toggle_vfo++;
    mock_radio.vfo = mock_radio.vfo == X6200_VFO_A ? X6200_VFO_B : X6200_VFO_A;
    return mock_radio.vfo;
}

uint16_t radio_change_vol(int16_t df) { mock_radio_calls.change_vol++; (void) df; return 0; }
uint16_t radio_change_moni(int16_t df) { (void) df; return 0; }
bool radio_change_spmode(int16_t df) { (void) df; return false; }

void radio_change_mute(void) { mock_radio_calls.change_mute++; }
bool radio_is_muted(void) { return false; }

void radio_set_pwr(float d) { mock_radio_calls.set_pwr++; (void) d; }

radio_charger_t radio_change_charger(int16_t d) { (void) d; return RADIO_CHARGER_OFF; }
x6200_mic_sel_t radio_change_mic(int16_t d) { mock_radio_calls.change_mic++; (void) d; return x6200_mic_builtin; }
uint8_t radio_change_hmic(int16_t d) { (void) d; return 0; }
uint8_t radio_change_imic(int16_t d) { (void) d; return 0; }

void radio_start_atu(void) { mock_radio_calls.start_atu++; }

/* El barrido de ROE (src/dialog_swrscan.c, issue #125) transmite mientras mide: aca NO hay
 * radio, asi que "arrancar el barrido" es solo el contador del mock y el dialogo recibe un
 * true que significa "el instrumento fake acepto". El `radio_set_freq` del mock guarda la
 * frecuencia y cuenta el comando, y `radio_stop_swrscan` cuenta el apagado: con eso el JSON
 * del arnes puede afirmar cuantos comandos salieron y que ninguno llego a un equipo. */
bool radio_start_swrscan(void) { mock_radio_calls.start_swrscan++; return true; }
void radio_stop_swrscan(void)  { mock_radio_calls.stop_swrscan++; }

/* The app-frame snapshot (issue #94): the harness renders single screens, it
 * never opens an app that mutates the radio, so the backup is a no-op here.
 * The symbol is needed because the real src/dialog.c calls it. */
void radio_state_save(void) {}
void radio_state_restore(void) {}

void radio_poweroff(void) { mock_radio_calls.poweroff++; }
void radio_set_ptt(bool tx) { mock_radio_calls.set_ptt++; (void) tx; }
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

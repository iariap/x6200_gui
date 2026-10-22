/*
 *  Explicit radio/hardware state for the host UI harness.
 *
 *  The GUI reads the radio state through radio.h, the cfg_cur/cfg subjects and
 *  a few small modules (bt_state, btaudio, wifi, backlight). None of that is
 *  available on the PC, so the harness owns a plain, explicit state instead:
 *  the visual layout only depends on the values below, which the scenario
 *  sets. No device, no network, no serial port is ever touched.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <aether_radio/x6200_control/control.h>

#include "radio.h"

typedef struct {
    /* What the scenario asks the "radio" to look like. */
    uint64_t        frequency_hz;   /* VFO-A / active VFO */
    uint64_t        tx_frequency_hz; /* split TX frequency */
    uint64_t        fft_width_hz;   /* spectrum span */
    x6200_mode_t    mode;
    x6200_vfo_t     vfo;
    bool            split;
    x6200_agc_t     agc;
    bool            att;
    bool            pre;
    bool            atu_enabled;
    int32_t         ant;            /* 1..3 */
    int32_t         smeter_db;      /* raw dB the S-meter draws */
    float           battery_v;
    float           external_v;
    uint8_t         battery_cap;    /* per cent */
    bool            charging;
    radio_state_t   state;
    /* Filtro DFL (issue #173, PR 1): el fixture declara low/high del modo y el
     * arnes calcula cfg_cur.filter.real.from/to con el mapeo de
     * src/cfg/mode.c::update_real_filters(). `filter_set` distingue "el fixture
     * lo declara" de "el default historico 0..3000", para no cambiar la banda de
     * los fixtures que no lo piden. */
    bool            filter_set;
    int32_t         filter_low;
    int32_t         filter_high;
} mock_radio_state_t;

extern mock_radio_state_t mock_radio;

/* Every call the GUI makes into the radio command surface while a scenario is
 * built. The render reads radio_get_state() (the frequency observers), so that
 * counter proves the mock was exercised; the command counters stay 0 unless a
 * fixture drives a press action. A functional gate can assert on them. */
typedef struct {
    unsigned get_state;
    unsigned set_freq;
    unsigned check_freq;
    unsigned toggle_vfo;
    unsigned set_ptt;
    unsigned set_pwr;
    unsigned change_vol;
    unsigned change_mic;
    unsigned change_mute;
    unsigned start_atu;
    unsigned poweroff;
    /* El barrido de ROE (issue #125): el arnes NO tiene radio, asi que estos dos
     * contadores son la prueba de que la unica "transmision" del fixture es el mock.
     * El binario no linkea aether_control: no hay camino a RF real desde aca. */
    unsigned start_swrscan;
    unsigned stop_swrscan;
} mock_radio_calls_t;

extern mock_radio_calls_t mock_radio_calls;

/* Sets mock_radio defaults and wires them into the cfg subjects. Called once
 * by the harness before the scenario builds the screen. */
void mock_radio_init(void);

/* Zeroes the call counters (called by mock_radio_init). */
void mock_radio_calls_reset(void);

/* Pushes the current mock_radio values into the subjects (so the observers run
 * as they would when the radio reports a change). */
void mock_radio_apply(void);

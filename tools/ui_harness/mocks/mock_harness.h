/*
 *  Harness-owned global state init. Each function belongs to one mock file.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>

/* mocks/mock_params.c: fills params_t with the values the layout needs. */
void mock_params_init(void);

/* mocks/mock_cfg.c: creates cfg / cfg_cur and its subjects. */
void mock_cfg_init(void);

/* mocks/mock_settings_manager.cpp: the last step of cfg_init() in the radio -the
 * remote control's mirror of the configuration (cfg_sm) getting its subjects- plus
 * the check that none of its parameters was left pointing at NULL (issue #30). */
void mock_settings_manager_check(void);

/* mocks/mock_main_screen.c: creates the keyboard group the real main screen
 * registers its focusable widget in (keyboard_init() is a no-op here). */
void mock_keyboard_group_up(void);

/* mocks/mock_btaudio.c: writes the /tmp file `btaudio-ctl list` would leave
 * (src/dialog_bt.c reads it back) with a fixed device list, so the Bluetooth
 * dialog renders real rows without running the script (`__wrap_system`). */
void mock_btaudio_seed_files(void);

/* mocks/mock_ui_deps.c: which of the two audio gain setters (src/audio.c) ran
 * last, and how many times each one did. The `settings-audio` fixture uses it to
 * check that each slider of Ajustes -> Audio reaches its own setter (issue #26). */
typedef enum {
    MOCK_AUDIO_NONE = 0,    /* ninguno todavia */
    MOCK_AUDIO_PLAY,        /* audio_set_play_vol() (salida) */
    MOCK_AUDIO_REC,         /* audio_set_rec_vol() (entrada) */
} mock_audio_setter_t;

mock_audio_setter_t mock_audio_last_setter(void);
float               mock_audio_last_db(void);
int                 mock_audio_calls(mock_audio_setter_t which);
void                mock_audio_reset(void);

/* mocks/mock_ui_deps.c: entrega sincrona del EVENT_MSG_UPDATE (issue #220).
 * Apagado por defecto: los fixtures de siempre no ven ningun evento y sus
 * pixeles no cambian. Un fixture que necesita leer el texto de un aviso lo
 * prende y despues deja correr el reloj virtual para que el timer de src/msg.c
 * escriba el label. */
void mock_event_bus_set_live(bool live);

/* mocks/mock_cfg.c: cuantas veces la GUI pidio las acciones de banda/VFO que el
 * arnes responde con un no-op (issue #220). Es el unico observable de que una
 * tecla bloqueada NO ejecuto la accion; sin los contadores, el no-op haria
 * pasar la asercion por vacuidad. */
void     mock_cfg_calls_reset(void);
unsigned mock_cfg_band_load_next_calls(void);
unsigned mock_cfg_vfo_copy_calls(void);

/* mocks/mock_dfl_store.c: doble en memoria del almacen de presets del DFL
 * (issue #183). El arnes no tiene params.db; el SQL real lo cubre el test
 * nativo. `mock_dfl_store_fail(true)` fuerza el fallo de escritura para el
 * fixture del feedback veraz. */
void mock_dfl_store_reset(void);
void mock_dfl_store_fail(bool on);

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "lvgl/lvgl.h"
#include "params/params.h"
#include "cfg/digital_modes.h"

#define MEM_HKEY_MAX_ID         9

#define MEM_BACKUP_ID   (MEM_HKEY_MAX_ID + 1)
#define MEM_FT8_ID      100
#define MEM_FT4_ID      200
#define MEM_WSPR_ID     300
#define MEM_SSTV_ID     400

lv_obj_t * main_screen();

void main_screen_notify_tx();
void main_screen_notify_rx();

void main_screen_keys_enable(bool value);
void main_screen_start_app(press_action_t page_app);

/* Nombre corto de la app abierta ("home" si no hay ninguna): lo usa el
 * registro de ultima respiracion del issue #121. */
const char *main_screen_current_app(void);
void main_screen_action(press_action_t action);

void main_screen_lock_freq(bool lock);

/* The X6100 apps show the channel/station name in a panel over the screen.
 * The channels dialog is not ported; it is shown as a message instead. */
void channel_overlay_show(const char *name);

/* The X6100 app dialogs close themselves and ask about the locks */
void main_screen_close_app(void);
bool main_screen_freq_locked(void);
bool main_screen_mode_locked(void);

/* Band lock: this GUI has none, so the remote server's band commands always go
 * through. The AGC one cycles the AGC the way the main screen does. */
bool main_screen_band_locked(void);
void main_screen_agc_next(void);

/* Encoder helpers the ported apps call, mapped to this GUI's vol/mfk model */
void main_screen_mfk(int dir);
void main_screen_vol_press(void);
void main_screen_lock_band(bool lock);
void main_screen_lock_mode(bool lock);
void main_screen_lock_ab(bool lock);

void main_screen_set_freq(uint64_t f);

void mem_load(uint16_t id);
void mem_save(uint16_t id);

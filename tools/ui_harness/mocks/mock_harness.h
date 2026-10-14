/*
 *  Harness-owned global state init. Each function belongs to one mock file.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

/* mocks/mock_params.c: fills params_t with the values the layout needs. */
void mock_params_init(void);

/* mocks/mock_cfg.c: creates cfg / cfg_cur and its subjects. */
void mock_cfg_init(void);

/* mocks/mock_main_screen.c: creates the keyboard group the real main screen
 * registers its focusable widget in (keyboard_init() is a no-op here). */
void mock_keyboard_group_up(void);

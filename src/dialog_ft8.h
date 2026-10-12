/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "dialog.h"

extern dialog_t *dialog_ft8;

/* Web remote: commands and state for the /remote page */
void dialog_ft8_remote(const char *cmd, int32_t val, const char *text);
int  dialog_ft8_remote_state(char *buf, size_t size);

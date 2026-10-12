/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdint.h>
#include "lvgl/lvgl.h"
#include "events.h"

#define ROT_VOL_EDIT_MODE 0
#define ROT_VOL_SELECT_MODE 1

#define ROT_MFK_INNER_DEFAULT_MODE 0
#define ROT_MFK_INNER_INVERSE_MODE 1

typedef struct {
    int             fd;
    uint16_t        left[3];
    uint16_t        right[3];
    uint8_t         mode;

    lv_indev_drv_t  indev_drv;
    lv_indev_t      *indev;
} rotary_t;

/* A detent as it comes from the knob: how much it moved and how long it took */
typedef struct {
    int16_t         diff;
    uint16_t        dt;
} rotary_data_t;

rotary_t * rotary_init(char *dev_name);

/* The main dial: read in its own thread so the speed per detent does not depend on
 * how busy the GUI loop is (the X6100's upstream fix ae2af22) */
void rotary_main_init(char *dev_name);

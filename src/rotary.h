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
#include "rotary_queue.h"

#define ROT_VOL_EDIT_MODE 0
#define ROT_VOL_SELECT_MODE 1

#define ROT_MFK_INNER_DEFAULT_MODE 0
#define ROT_MFK_INNER_INVERSE_MODE 1

typedef struct {
    int             fd;
    uint16_t        left[3];
    uint16_t        right[3];
    uint8_t         mode;

    /* Estado POR INSTANCIA (issue #186): antes eran `static` de archivo y los
     * tres encoders (VOL, MFK, MFK interno) compartian el remanente y el estado
     * de tecla. Cada rotary_t tiene los suyos. */
    int32_t             remain_diff;
    lv_indev_state_t    prev_state;

    lv_indev_drv_t  indev_drv;
    lv_indev_t      *indev;
} rotary_t;

rotary_t * rotary_init(char *dev_name);

/* The main dial: read in its own thread so the speed per detent does not depend on
 * how busy the GUI loop is (the X6100's upstream fix ae2af22). */
void rotary_main_init(char *dev_name);

/* Reinicia el motor adaptativo del dial principal (issue #186): el proximo
 * detent es x1. Lo piden main_screen (lock, paso, VFO, banda, modo, cambio
 * externo de frecuencia) y Settings (cambio de umbral). Se consume al inicio
 * del proximo drenaje, en el hilo de la GUI. */
void rotary_tuning_reset(void);

/* Diagnostico de la cola del dial principal (issue #186, seccion 3). */
void rotary_tuning_queue_stats(uint32_t *high_watermark, uint32_t *queue_full_waits);

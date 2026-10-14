/*
 *  Headless display for the host UI harness.
 *
 *  A RAM framebuffer the size of the radio screen (800x480) registered as an
 *  LVGL display: the component tree is rendered by LVGL's own draw pipeline
 *  and the flush callback copies the pixels into the buffer. No X11, Wayland,
 *  GPU or host display is involved.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

#include "lvgl/lvgl.h"

#define HARNESS_SCREEN_W 800
#define HARNESS_SCREEN_H 480

/* Registers the LVGL display. Call after lv_init(). */
void display_init(void);

/* The rendered image, HARNESS_SCREEN_W * HARNESS_SCREEN_H lv_color_t. */
const lv_color_t *display_framebuffer(void);

uint32_t display_width(void);
uint32_t display_height(void);

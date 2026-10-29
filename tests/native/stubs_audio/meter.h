/*
 * Meter stub for the capture-callback stage (issue #351, parte 2): src/audio.c incluye
 * "meter.h" (que en el firmware arrastra LVGL) pero el callback no usa el S-meter.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

int16_t meter_get_raw_db(void);

/* Hooks que los stubs (tests/waterfall/stubs.c) exponen al test del waterfall
 * (OPT-2, #148). SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Cuando esta en true, la proxima llamada a lv_img_buf_alloc() devuelve NULL
 * (el link va con --wrap=lv_img_buf_alloc), para probar el camino de reserva
 * fallida. La baja el propio wrapper. */
extern bool test_fail_img_alloc;

void test_backlight_set(bool on);
void test_palette_set(uint8_t index, uint32_t rgb);
void test_palette_fill(uint32_t (*fn)(uint8_t));

/*
 * Surface of tests/native/clock_stubs.c used by tests/native/test_clock.c.
 *
 * One entry per object src/clock.c::clock_init() creates, in creation order:
 * the panel container, the "8 fantasma" (only with a segmented digit family),
 * the clock line's value, and the status line's three labels. The test reads
 * each role's text, font, colour, flags, alignment and parent from here, so the
 * contract is pinned without modelling LVGL's layout (that is what the
 * tools/ui_harness `--inspect` gates measure with the real LVGL).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

/* Orden de creacion (src/clock.c::clock_init). El icono se crea DESPUES del
 * porcentaje porque se ancla a el (LV_ALIGN_OUT_LEFT_BOTTOM): el porcentaje es
 * el que fija el borde derecho de la banda. */
enum {
    CLOCK_STUB_CONTAINER = 0, /* el panel: clock_style, sin input */
    CLOCK_STUB_GHOST     = 1, /* "8 fantasma" de la linea del reloj */
    CLOCK_STUB_TIME      = 2, /* HH:MM:SS */
    CLOCK_STUB_EXT       = 3, /* alimentacion externa (opcional) */
    CLOCK_STUB_PCT       = 4, /* porcentaje */
    CLOCK_STUB_ICON      = 5, /* icono de bateria */
    CLOCK_STUB_OBJECTS   = 6,
};

int clock_stub_object_count(void);
int clock_stub_parent(int index);
int clock_stub_timer_count(void);

bool clock_stub_is_label(int index);
const char * clock_stub_text(int index);
const lv_font_t * clock_stub_font(int index);
lv_color_t clock_stub_color(int index);
lv_opa_t clock_stub_text_opa(int index);
bool clock_stub_hidden(int index);
uint32_t clock_stub_flags(int index);
bool clock_stub_style_removed(int index);
int clock_stub_style_adds(int index);
bool clock_stub_style_is_clock(int index);
lv_opa_t clock_stub_bg_img_opa(int index);
const void * clock_stub_bg_img_src(int index);
lv_coord_t clock_stub_width(int index);
lv_coord_t clock_stub_height(int index);
lv_align_t clock_stub_align(int index);
lv_coord_t clock_stub_x_ofs(int index);
lv_coord_t clock_stub_y_ofs(int index);
bool clock_stub_aligned_to(int index);
int clock_stub_align_base(int index);

const char * clock_stub_time_text(void);
const char * clock_stub_shadow_text(void);
const char * clock_stub_ext_text(void);
const char * clock_stub_bat_icon_text(void);
const char * clock_stub_bat_pct_text(void);

/* Corre los callbacks que clock_init() registro (show_time y time-sync). */
void clock_stub_run_timers(void);

/* Contadores de escrituras (OPT-4 #150: una vuelta con el mismo valor no
 * vuelve a escribir ni a invalidar). */
int clock_stub_label_set_calls(void);
int clock_stub_style_font_calls(void);
int clock_stub_style_color_calls(void);
int clock_stub_style_bg_calls(void);

/* Estado que el test maneja a mano: el reloj de pared (o su fallo), y el estado
 * de la radio. */
void clock_stub_set_wall_clock(int hour, int min, int sec);
void clock_stub_set_localtime_null(bool is_null);
void clock_stub_set_radio_state(int state);

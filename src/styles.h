/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

/* Dialog area: the 800x480 screen the ported apps were laid out for */
#ifndef BUTTONS_H
#define BUTTONS_H           58      /* bottom button bar */
#endif

/* Y del borde de arriba del area de dialogo: la barra de estado de arriba termina en 66 (de ahi
 * arranca dialog_style). Estaba en 108, de la pantalla del X6100, y los dialogos que se alineaban
 * con este valor (WeFax, SSTV) quedaban 42 px mas abajo con su ultima linea debajo de los botones
 * (medido con capturas, docs/HALLAZGOS.md 21). */
#ifndef DIALOG_Y
#define DIALOG_Y            66
#endif

#ifndef DIALOG_H
#define DIALOG_H            372     /* valor viejo: lo que asumen las apps (marco propio) */
#endif

/* Alto REAL de la caja de dialogo, el que usa dialog_style: la pantalla tiene 480, la barra de
 * arriba termina en 66 y la fila de botones mide 62, asi que arriba de los botones hay 352 y la
 * caja mide 348. DIALOG_H quedo en 372 de cuando los botones median 42: los dialogos que se
 * apoyaban ahi calculaban 24 px de mas y su ultima linea quedaba debajo de los botones
 * (medido con capturas, docs/HALLAZGOS.md 21). */
#ifndef DIALOG_BOX_H
#define DIALOG_BOX_H        348
#endif

/* Tallest dialog area: for static buffers (as in the X6100) */
#ifndef DIALOG_H_MAX
#define DIALOG_H_MAX        372
#endif

#include "params/params.h"

#include <unistd.h>
#include "lvgl/lvgl.h"

// COLORS

#define COLOR_LIGHT_GREEN (0x80FF80)
#define COLOR_LIGHT_YELLOW (0xFFFF30)
#define COLOR_LIGHT_RED (0xFF8080)

/* Opacidad del "8 fantasma": la capa de segmentos apagados que los paneles de
 * digitos (la fila de frecuencia y el reloj) dibujan DEBAJO del valor, con la
 * familia segmentada (src/clock.c, src/main_screen.c). El fantasma no pisa el
 * valor: comparte sus segmentos, asi que lo unico que decide este numero es
 * cuanta luz tienen los segmentos que el valor no enciende. Vive aca y no en
 * ui_tokens.h porque es una constante de la capa de estilos y este header es el
 * unico que src/clock.c comparte con su test nativo. */
#define UI_DIGIT_SHADOW_OPA  LV_OPA_30

// Symbols

#define SYMBOL_PLUG "\xEF\x87\xA6"  // F1E6
#define SYMBOL_PLUG_CHARGE "\xEE\x95\x9B"  // E55B

// Pallete

extern const uint32_t *wf_palette;

extern lv_color_t   bg_color;

extern lv_style_t   background_style;
extern lv_style_t   spectrum_style;
extern lv_style_t   freq_style;
extern lv_style_t   freq_main_style;
extern lv_style_t   waterfall_style;
extern lv_style_t   btn_style;
extern lv_style_t   btn_active_style;
extern lv_style_t   btn_disabled_style;
extern lv_style_t   msg_style;
extern lv_style_t   msg_tiny_style;
extern lv_style_t   clock_style;
extern lv_style_t   info_style;
extern lv_style_t   info_item_style;
extern lv_style_t   meter_style;
extern lv_style_t   tx_info_style;

extern lv_style_t   panel_top_style;
extern lv_style_t   panel_mid_style;
extern lv_style_t   panel_bottom_style;
extern lv_style_t   pannel_style;
#define panel_style pannel_style    /* the X6100 spelling */

/* The decoded-text panel's info line; this GUI has no separate style for it */
#define panel_info_style pannel_style

extern lv_style_t   dialog_style;
extern lv_style_t   dialog_item_style;
extern lv_style_t   dialog_item_focus_style;
extern lv_style_t   dialog_item_edited_style;
extern lv_style_t   dialog_dropdown_list_style;

extern lv_style_t   cw_tune_style;
extern lv_style_t   eq_style;

extern lv_font_t    sony_14;
extern lv_font_t    sony_16;
extern lv_font_t    sony_18;
extern lv_font_t    sony_20;
extern lv_font_t    sony_22;
extern lv_font_t    sony_24;
extern lv_font_t    sony_26;
extern lv_font_t    sony_28;
extern lv_font_t    sony_30;
extern lv_font_t    sony_32;
extern lv_font_t    sony_34;
extern lv_font_t    sony_36;
extern lv_font_t    sony_38;
extern lv_font_t    sony_40;
extern lv_font_t    sony_42;
extern lv_font_t    sony_44;
extern lv_font_t    sony_60;

/* The family the digit panels draw with (src/fonts/font_select.c). Declared here
 * as well as in fonts/font_select.h because src/clock.c draws the clock's digits
 * and the clock test compiles it against tests/native/stubs_clock/styles.h, which
 * has no fonts/ tree. */
const struct _lv_font_t *font_digit(const struct _lv_font_t *stock_slot);
bool font_digit_active(void);
void fonts_set_digit_family(uint8_t digit_family);

void styles_init(themes_t theme);

void styles_set_theme(themes_t theme);

/* Los switches y sliders los pintaba el tema de LVGL (riel #E0E0E0, knob del color primario): el
 * riel daba 2.78:1 contra el dialogo y en algunos temas el knob del slider quedaba igual que el
 * relleno. Se pintan por partes con roles del tema, que el chequeo WCAG valida en los 9 temas
 * (docs/HALLAZGOS.md 28). */
void styles_switch_init(lv_obj_t *sw);
void styles_slider_init(lv_obj_t *sl);

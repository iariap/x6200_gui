/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#define BUTTONS 5

#ifdef __cplusplus
#include "cfg/subjects.h"

extern "C" {
#endif

#include "lvgl/lvgl.h"
#include "params/params.h"
#include "main_screen.h"
#include "mfk.h"
#include "vol.h"

typedef enum {
    BTN_EMPTY,
    BTN_TEXT,
    BTN_TEXT_FN,
} btn_type_t;

typedef struct button_item_t {
    /* Fields the ported X6100 app dialogs set (see button_data_t below) */
    bool                   mark;
    bool                   disabled;
    btn_type_t type;
    union {
        const char *label;
        const char *(*label_fn)();
    };
    void (*press)(struct button_item_t *);
    void (*hold)(struct button_item_t *);
    const char *voice;
    // next/prev page
    struct buttons_page_t *next;
    struct buttons_page_t *prev;
    union {
        int32_t            data;        /* this GUI's name for it */
        int32_t            ctrl;        /* the X6100 name the ported dialogs use */
    };
    /* The X6100 lets the encoder change the value of the focused app button
     * (WeFax align/tilt); accepted here, the registry is not ported yet */
    bool                   encoder_allowed;
    union {
        lv_obj_t          *label_obj;   /* this GUI's name for it */
        lv_obj_t          *disp_btn;    /* the X6100 name the ported dialogs use */
    };
    Subject               **subj;
    Observer              *observer;
    /* Ruta TACTIL de la barra inferior (issue #183): la celda responde a
     * LV_EVENT_CLICKED/LV_EVENT_LONG_PRESSED ademas de las teclas fisicas F1..F5.
     * Solo lo piden los favoritos 1-3; el resto de las paginas queda como estaba
     * (la barra es una leyenda de teclas, no un pad). */
    bool                   touch_hold;
} button_item_t;

typedef struct buttons_page_t {
    button_item_t *items[BUTTONS];
} buttons_page_t;

typedef buttons_page_t *buttons_group_t[];

/* The X6100 firmware renamed button_item_t to button_data_t when it added the
 * app button pages; the layout is the same, so the ported dialogs use theirs */
typedef button_item_t button_data_t;

extern buttons_page_t buttons_page_vol_1;

extern buttons_page_t buttons_page_msg_cw_1;
extern buttons_page_t buttons_page_msg_cw_2;

extern buttons_page_t buttons_page_rtty;

extern buttons_group_t buttons_group_gen;
extern buttons_group_t buttons_group_app;
extern buttons_group_t buttons_group_key;
extern buttons_group_t buttons_group_dfn;
extern buttons_group_t buttons_group_dfl;

/* Paginas DFL (issue #179): la normal lleva las cinco celdas de siempre
 * (LOW/HIGH/BW con valores + Eq + "Filtro grafico"), y la expandida cambia las
 * tres primeras por nombres sin cifras (seleccionan el parametro para el encoder
 * VOL) y la quinta por "Waterfall" (contrae). dfl_inline.c usa la expandida al
 * abrir el editor inline. */
extern buttons_page_t buttons_page_dfl_1;
extern buttons_page_t buttons_page_dfl_expanded;

/* --- Paginas DFL (issue #183) ----------------------------------------------
 * A = Favoritos (tres presets rapidos del modo + Ajustar + Mas), B = Ajustar
 * (buttons_page_dfl_1), C = Grafico inline (buttons_page_dfl_expanded),
 * D = Mas (Mem 1/2, Guardar 1/2, Volver), E = Confirmar guardado,
 * F = Elegir origen de un favorito. La pagina se deduce de la pagina cargada;
 * no hay estado duplicado. */
typedef enum {
    DFL_PAGE_NONE = 0,
    DFL_PAGE_FAVORITES,   /* A */
    DFL_PAGE_ADJUST,      /* B */
    DFL_PAGE_GRAPH,       /* C */
    DFL_PAGE_MORE,        /* D */
    DFL_PAGE_CONFIRM,     /* E */
    DFL_PAGE_ASSIGN,      /* F */
} dfl_page_t;

/* DFL corto desde fuera del DFL: abre A (o B si el modo no tiene catalogo). */
void       buttons_dfl_open(void);

/* DFL corto con el DFL abierto: cancela suboperaciones no confirmadas y vuelve
 * siempre a A (contrae el grafico si estaba en C). */
void       buttons_dfl_short(void);

/* C = Grafico se carga desde el editor inline (src/dfl_inline.c, que tambien se
 * abre directo por el long press DFL). Fija la familia vigente y la pagina en un
 * solo lugar: sin eso un cambio de modo de la MISMA familia se lee como familia
 * nueva y cierra el editor (issue #182/#183). */
void       buttons_dfl_graph_open(void);

/* ESC con el DFL abierto. */
void       buttons_dfl_escape(void);

bool       buttons_dfl_active(void);
dfl_page_t buttons_dfl_page(void);

/* Re-lee Subjects y rearma etiquetas, marcas, disabled y banda contextual. */
void       buttons_dfl_refresh(void);

/* Cambio de modo: misma familia no toca nada; familia nueva cancela E/F y va a
 * A (o a B si la familia nueva no admite presets). Nunca aplica un preset. */
void       buttons_dfl_on_mode_change(void);

/* TX/RX: los presets y aplicar Mem quedan deshabilitados; la navegacion sigue. */
void       buttons_dfl_on_radio(bool tx);

/* Seam de test (#183): resultado del ultimo guardado. El arnes no dibuja el
 * overlay de mensajes (el bus de eventos es mock), asi que el "feedback veraz"
 * se comprueba por aca: NUNCA se informa OK si la fila no entro. */
typedef enum {
    DFL_SAVE_FEEDBACK_NONE = 0,
    DFL_SAVE_FEEDBACK_OK,
    DFL_SAVE_FEEDBACK_FAILED,
} dfl_save_feedback_t;

dfl_save_feedback_t buttons_dfl_last_save(void);

/* Alto de la barra de botones (5x160x62): el editor DFL inline lo usa para
 * derivar su alto real sin hardcodear 240. */
uint8_t buttons_bar_height(void);

/* Marca en la barra expandida la celda del parametro seleccionado (0=LOW,
 * 1=HIGH, 2=BW); -1 no marca ninguna. */
void buttons_dfl_expanded_sync(int sel);

/* Seam de test (issue #183): el objeto de la celda n (0..4) de la barra inferior,
 * para que el arnes mande los eventos LVGL reales de la ruta tactil
 * (LV_EVENT_CLICKED / LV_EVENT_LONG_PRESSED). En la radio no se usa. */
lv_obj_t *buttons_cell_obj(uint8_t n);

// TODO: move to applications
extern buttons_group_t buttons_group_msg_cw;
extern buttons_group_t buttons_group_msg_voice;

void            buttons_init(lv_obj_t *parent);
void            buttons_refresh(button_item_t *item);
void            buttons_mark(button_item_t *item, bool on);
void            buttons_disabled(button_item_t *item, bool on);
void            buttons_load(uint8_t n, button_item_t *item);
void            buttons_load_page(buttons_page_t *page);
void            buttons_unload_page();
void            button_next_page_cb(button_item_t *item);
void            button_prev_page_cb(button_item_t *item);
void            button_update_label(button_item_t *item, char *label);
void            buttons_press(uint8_t n, bool hold);
void            buttons_load_page_group(buttons_group_t group);
buttons_page_t *buttons_get_cur_page();

#ifdef __cplusplus
}
#endif

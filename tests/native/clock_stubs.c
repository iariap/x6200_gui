/*
 * Stub implementations for the native clock test (tests/native/test_clock.c).
 *
 * They stand in for the small set of LVGL/firmware symbols src/clock.c calls,
 * and expose just enough state for the test to drive the clock the way the
 * firmware does: run the timer callbacks `clock_init()` registers and read back
 * what each object of the panel was asked to draw.
 *
 * Issue #250 (epic #249): the panel is now a container plus per-role labels --
 * the clock line (value + "8 fantasma") and the status line (external supply,
 * battery icon, percentage). The stub records one entry per created object, in
 * creation order, so the test can pin each role's text, font, colour, flags and
 * alignment. The pixel geometry is NOT modelled here: it is measured with the
 * real LVGL in tools/ui_harness (`--inspect`, `tests/check_*`).
 *
 * `localtime()` is the stub's, not the host's: the test drives the wall clock
 * (and its failure path) without depending on the machine's clock or TZ.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "styles.h"
#include "theme.h"
#include "radio.h"
#include "util.h"
#include "voice.h"
#include "msg.h"
#include "params/params.h"

#include <time.h>

params_t params;

lv_style_t clock_style;
lv_font_t sony_16;
lv_font_t sony_20;
lv_font_t sony_30;
lv_font_t sony_38;

/* src/fonts/font_select.c is not compiled here: the clock test only needs the
 * identity (the ROM's segmented digit families are host-side data). The active
 * flag is true, which is the case this test covers: the clock panels drawn with
 * a segmented family, the only case that has an "8 fantasma" layer. */
const lv_font_t *font_digit(const lv_font_t *stock_slot) {
    return stock_slot;
}

bool font_digit_active(void) {
    return true;
}

void fonts_set_digit_family(uint8_t family) {
    (void) family;
}

/* Every role gets its own value, so the test can tell which one a label was
 * painted with (src/theme.h: the code asks for a role, never a colour). */
uint32_t theme_hex(theme_color_t c) {
    switch (c) {
        case TC_text:     return 0xFFF8EEFFu;   /* Aurora's text */
        case TC_text_dim: return 0xFFD5C4E0u;   /* Aurora's text_dim */
        case TC_warn:     return 0xFFFFD196u;   /* Aurora's warn */
    }
    return 0x00000000u;
}

lv_color_t lv_color_hex(uint32_t c) {
    lv_color_t color;
    color.red   = (uint8_t) ((c >> 16) & 0xFF);
    color.green = (uint8_t) ((c >> 8) & 0xFF);
    color.blue  = (uint8_t) (c & 0xFF);
    color.alpha = 0xFF;
    return color;
}

/* ---- objects ----------------------------------------------------------------- */

#define STUB_MAX_OBJ        8
#define STUB_MAX_LABEL_TEXT 64

typedef struct {
    bool             is_label;
    bool             exists;
    int              parent;
    char             text[STUB_MAX_LABEL_TEXT];
    const lv_font_t *font;
    lv_color_t       color;
    lv_opa_t         text_opa;
    lv_opa_t         bg_img_opa;
    const void      *bg_img_src;
    uint32_t         flags;
    bool             style_removed;
    int              style_adds;
    bool             style_is_clock;
    lv_coord_t       w;
    lv_coord_t       h;
    lv_align_t       align;
    lv_coord_t       x_ofs;
    lv_coord_t       y_ofs;
    bool             aligned_to;
    int              align_base;
} stub_obj_t;

static stub_obj_t stub_objs[STUB_MAX_OBJ];
static lv_obj_t   stub_handles[STUB_MAX_OBJ];
static int        stub_obj_count;

/* Contadores de escrituras: el test de OPT-4 #150 comprueba que una segunda
 * vuelta con el mismo valor NO vuelve a escribir. `align_to_calls` cuenta las
 * llamadas a `lv_obj_align_to` (el re-ancla del icono de bateria): el mismo test
 * fija que en steady state (el porcentaje no cambia) NO se re-ancla. */
static int label_set_calls;
static int style_font_calls;
static int style_color_calls;
static int style_bg_calls;
static int align_to_calls;

static lv_timer_t stub_timer;

/* clock_init() registers the clock callback first (show_time, 500 ms) and the
 * time-sync callback second; both are kept so the test can run them. */
#define STUB_MAX_TIMERS 4
static void (*stub_timer_cbs[STUB_MAX_TIMERS])(lv_timer_t *);
static int stub_timer_count;

/* The wall clock the stub's localtime() reports, and whether it fails. */
static int  stub_hour = 12, stub_min = 34, stub_sec = 56;
static bool stub_localtime_null;

static int stub_radio_state = RADIO_RX;

static int stub_index(const lv_obj_t * obj) {
    for (int i = 0; i < stub_obj_count; i++) {
        if (obj == &stub_handles[i]) return i;
    }
    return 0;
}

static lv_obj_t * stub_new(bool is_label, lv_obj_t * parent) {
    if (stub_obj_count >= STUB_MAX_OBJ) {
        return &stub_handles[STUB_MAX_OBJ - 1];
    }

    int i = stub_obj_count++;
    stub_obj_t * o = &stub_objs[i];

    memset(o, 0, sizeof(*o));
    o->is_label  = is_label;
    o->exists    = true;
    o->parent    = (parent == NULL) ? -1 : stub_index(parent);
    o->text_opa  = LV_OPA_COVER;
    o->bg_img_opa = LV_OPA_TRANSP;
    o->align     = LV_ALIGN_TOP_LEFT;
    o->w         = LV_SIZE_CONTENT;
    o->h         = LV_SIZE_CONTENT;
    o->align_base = -1;

    /* lv_obj_class.c: un lv_obj nace clickable, snappeable y focusable por
     * click; lv_label.c se lo quita. El reloj limpia lo que no quiere. */
    o->flags = is_label ? 0u
                        : (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE |
                           LV_OBJ_FLAG_CLICK_FOCUSABLE);

    return &stub_handles[i];
}

lv_obj_t * lv_obj_create(lv_obj_t * parent) {
    return stub_new(false, parent);
}

lv_obj_t * lv_label_create(lv_obj_t * parent) {
    return stub_new(true, parent);
}

void lv_obj_remove_style_all(lv_obj_t * obj) {
    stub_objs[stub_index(obj)].style_removed = true;
}

void lv_obj_add_style(lv_obj_t * obj, const lv_style_t * style, uint32_t selector) {
    (void) selector;
    stub_obj_t * o = &stub_objs[stub_index(obj)];
    o->style_adds++;
    if (style == &clock_style) o->style_is_clock = true;
}

void lv_obj_add_flag(lv_obj_t * obj, uint32_t flag) {
    stub_objs[stub_index(obj)].flags |= flag;
}

void lv_obj_clear_flag(lv_obj_t * obj, uint32_t flag) {
    stub_objs[stub_index(obj)].flags &= ~flag;
}

bool lv_obj_has_flag(const lv_obj_t * obj, uint32_t flag) {
    return (stub_objs[stub_index(obj)].flags & flag) != 0u;
}

void lv_obj_set_size(lv_obj_t * obj, lv_coord_t w, lv_coord_t h) {
    stub_obj_t * o = &stub_objs[stub_index(obj)];
    o->w = w;
    o->h = h;
}

void lv_obj_align(lv_obj_t * obj, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs) {
    stub_obj_t * o = &stub_objs[stub_index(obj)];
    o->align     = align;
    o->x_ofs     = x_ofs;
    o->y_ofs     = y_ofs;
    o->aligned_to = false;
}

void lv_obj_align_to(lv_obj_t * obj, const lv_obj_t * base, lv_align_t align, lv_coord_t x_ofs,
                     lv_coord_t y_ofs) {
    stub_obj_t * o = &stub_objs[stub_index(obj)];
    o->align      = align;
    o->x_ofs      = x_ofs;
    o->y_ofs      = y_ofs;
    o->aligned_to = true;
    o->align_base = stub_index(base);
    align_to_calls++;
}

void lv_obj_set_style_text_font(lv_obj_t * obj, const lv_font_t * font, uint32_t selector) {
    (void) selector;
    stub_objs[stub_index(obj)].font = font;
    style_font_calls++;
}

void lv_obj_set_style_text_color(lv_obj_t * obj, lv_color_t color, uint32_t selector) {
    (void) selector;
    stub_objs[stub_index(obj)].color = color;
    style_color_calls++;
}

void lv_obj_set_style_text_align(lv_obj_t * obj, lv_text_align_t align, uint32_t selector) {
    (void) obj;
    (void) align;
    (void) selector;
}

void lv_obj_set_style_text_opa(lv_obj_t * obj, lv_opa_t value, uint32_t selector) {
    (void) selector;
    stub_objs[stub_index(obj)].text_opa = value;
}

void lv_obj_set_style_bg_img_opa(lv_obj_t * obj, lv_opa_t value, uint32_t selector) {
    (void) selector;
    stub_objs[stub_index(obj)].bg_img_opa = value;
}

void lv_obj_set_style_bg_img_src(lv_obj_t * obj, const void * src, uint32_t selector) {
    (void) selector;
    stub_objs[stub_index(obj)].bg_img_src = src;
    style_bg_calls++;
}

/* API de la version previa del reloj (el `pad_ver 18` que centraba el valor y su
 * fantasma): el reloj del #250 no la llama, y el stub la deja sin efecto para
 * poder compilar la revision anterior de src/clock.c con este mismo test. */
lv_coord_t lv_obj_get_style_pad_top(const lv_obj_t * obj, uint32_t selector) {
    (void) obj;
    (void) selector;

    return 0;
}

void lv_obj_set_style_pad_ver(lv_obj_t * obj, lv_coord_t value, uint32_t selector) {
    (void) obj;
    (void) value;
    (void) selector;
}

const lv_font_t * lv_obj_get_style_text_font(const lv_obj_t * obj, uint32_t selector) {
    (void) selector;
    return stub_objs[stub_index(obj)].font;
}

lv_color_t lv_obj_get_style_text_color(const lv_obj_t * obj, uint32_t selector) {
    (void) selector;
    return stub_objs[stub_index(obj)].color;
}

const void * lv_obj_get_style_bg_img_src(const lv_obj_t * obj, uint32_t selector) {
    (void) selector;

    const stub_obj_t * o = &stub_objs[stub_index(obj)];

    if (o->bg_img_src != NULL) return o->bg_img_src;
    /* El stub no modela el CONTENIDO de clock_style (el bitmap del panel que el
     * tema elige): devuelve el estilo mismo como sentinela de su imagen, que es
     * lo que src/clock.c copia a la etiqueta del valor. */
    return o->style_is_clock ? (const void *) &clock_style : NULL;
}

void lv_label_set_text(lv_obj_t * obj, const char * text) {
    snprintf(stub_objs[stub_index(obj)].text, STUB_MAX_LABEL_TEXT, "%s", text);
    label_set_calls++;
}

const char * lv_label_get_text(const lv_obj_t * obj) {
    return stub_objs[stub_index(obj)].text;
}

lv_timer_t * lv_timer_create(void (*timer_cb)(lv_timer_t *), uint32_t period, void * user_data) {
    (void) period;
    (void) user_data;
    if (stub_timer_count < STUB_MAX_TIMERS) {
        stub_timer_cbs[stub_timer_count++] = timer_cb;
    }
    return &stub_timer;
}

uint64_t get_time(void) {
    return 0;
}

radio_state_t radio_get_state(void) {
    return (radio_state_t) stub_radio_state;
}

/* El reloj de pared del test: localtime() NUNCA es el de la libc, ni depende de
 * TZ, y el test puede hacerlo fallar (contrato 2 del issue #250). */
struct tm * localtime(const time_t * t) {
    (void) t;

    if (stub_localtime_null) {
        return NULL;
    }

    static struct tm tm_value;

    tm_value.tm_year = 126; /* 2026 */
    tm_value.tm_mon  = 9;
    tm_value.tm_mday = 23;
    tm_value.tm_hour = stub_hour;
    tm_value.tm_min  = stub_min;
    tm_value.tm_sec  = stub_sec;

    return &tm_value;
}

void voice_sure(void) {}

void voice_say_float(const char * prompt, float x) {
    (void) prompt;
    (void) x;
}

void msg_schedule_text_fmt(const char * fmt, ...) {
    (void) fmt;
}

void params_lock(void) {}

void params_unlock(bool * dirty) {
    if (dirty) {
        *dirty = true;
    }
}

/* ---- accessors --------------------------------------------------------------- */

/* El orden de creacion de src/clock.c::clock_init(): el contenedor, el "8
 * fantasma" (solo con familia segmentada), la hora, la alimentacion externa, el
 * porcentaje y el icono de bateria (que se ancla al porcentaje). */
enum {
    CLOCK_STUB_CONTAINER = 0,
    CLOCK_STUB_GHOST     = 1,
    CLOCK_STUB_TIME      = 2,
    CLOCK_STUB_EXT       = 3,
    CLOCK_STUB_PCT       = 4,
    CLOCK_STUB_ICON      = 5,
    CLOCK_STUB_OBJECTS   = 6,
};

int clock_stub_object_count(void) {
    return stub_obj_count;
}

int clock_stub_parent(int index) {
    return stub_objs[index].parent;
}

int clock_stub_timer_count(void) {
    return stub_timer_count;
}

bool clock_stub_is_label(int index) {
    return stub_objs[index].is_label;
}

const char * clock_stub_text(int index) {
    return stub_objs[index].text;
}

const lv_font_t * clock_stub_font(int index) {
    return stub_objs[index].font;
}

lv_color_t clock_stub_color(int index) {
    return stub_objs[index].color;
}

lv_opa_t clock_stub_text_opa(int index) {
    return stub_objs[index].text_opa;
}

bool clock_stub_hidden(int index) {
    return (stub_objs[index].flags & LV_OBJ_FLAG_HIDDEN) != 0u;
}

uint32_t clock_stub_flags(int index) {
    return stub_objs[index].flags;
}

bool clock_stub_style_removed(int index) {
    return stub_objs[index].style_removed;
}

int clock_stub_style_adds(int index) {
    return stub_objs[index].style_adds;
}

bool clock_stub_style_is_clock(int index) {
    return stub_objs[index].style_is_clock;
}

lv_opa_t clock_stub_bg_img_opa(int index) {
    return stub_objs[index].bg_img_opa;
}

const void * clock_stub_bg_img_src(int index) {
    const stub_obj_t * o = &stub_objs[index];

    if (o->bg_img_src != NULL) return o->bg_img_src;

    return o->style_is_clock ? (const void *) &clock_style : NULL;
}

lv_coord_t clock_stub_width(int index) {
    return stub_objs[index].w;
}

lv_coord_t clock_stub_height(int index) {
    return stub_objs[index].h;
}

lv_align_t clock_stub_align(int index) {
    return stub_objs[index].align;
}

lv_coord_t clock_stub_x_ofs(int index) {
    return stub_objs[index].x_ofs;
}

lv_coord_t clock_stub_y_ofs(int index) {
    return stub_objs[index].y_ofs;
}

bool clock_stub_aligned_to(int index) {
    return stub_objs[index].aligned_to;
}

int clock_stub_align_base(int index) {
    return stub_objs[index].align_base;
}

const char * clock_stub_time_text(void) {
    return clock_stub_text(CLOCK_STUB_TIME);
}

const char * clock_stub_shadow_text(void) {
    return clock_stub_text(CLOCK_STUB_GHOST);
}

const char * clock_stub_ext_text(void) {
    return clock_stub_text(CLOCK_STUB_EXT);
}

const char * clock_stub_bat_icon_text(void) {
    return clock_stub_text(CLOCK_STUB_ICON);
}

const char * clock_stub_bat_pct_text(void) {
    return clock_stub_text(CLOCK_STUB_PCT);
}

void clock_stub_run_timers(void) {
    for (int i = 0; i < stub_timer_count; i++) {
        stub_timer_cbs[i](&stub_timer);
    }
}

int clock_stub_label_set_calls(void) {
    return label_set_calls;
}

int clock_stub_style_font_calls(void) {
    return style_font_calls;
}

int clock_stub_style_color_calls(void) {
    return style_color_calls;
}

int clock_stub_style_bg_calls(void) {
    return style_bg_calls;
}

int clock_stub_align_to_calls(void) {
    return align_to_calls;
}

void clock_stub_set_wall_clock(int hour, int min, int sec) {
    stub_hour = hour;
    stub_min  = min;
    stub_sec  = sec;
}

void clock_stub_set_localtime_null(bool is_null) {
    stub_localtime_null = is_null;
}

void clock_stub_set_radio_state(int state) {
    stub_radio_state = state;
}

/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Raster del arranque neo-retro (issue #192), sin hardware y sin heap.
 *
 *  Todo lo que decide que se ve (caracteres del logo, cursor, textos, cajas) es
 *  una funcion pura de (frame, tiempo); todo lo que escribe lo hace sobre el
 *  buffer logico RGB565 que recibe. El backend fb.c es el unico que conoce el
 *  framebuffer fisico.
 */
#include "renderer.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Textos del contrato. Todos en ingles y sin indicadores de progreso.
 * --------------------------------------------------------------------------- */
#define BA_LOGO_TEXT     "X6200"
#define BA_HEADER_TEXT   "X6200 / SYSTEM STARTUP"
#define BA_SUBTITLE_TEXT "HF TRANSCEIVER"
#define BA_FOOTER_TEXT   "> INITIALIZING_"
#define BA_DELAYED_TEXT  "> STARTUP DELAYED_"
#define BA_ALPHABET      "#*+?0123456789ABCDEF"
#define BA_CURSOR_CHAR   '_'

/* ---------------------------------------------------------------------------
 * Fuente mono 5x7: solo los glifos que usan los textos de arriba y el alfabeto
 * provisional. Cada glifo son 5 columnas; el bit 0 es la fila de arriba.
 * --------------------------------------------------------------------------- */
static const char    ba_font_chars[] = " #*+?/>_0123456789ABCDEFGHILMNPRSTUVXYZ";
static const uint8_t ba_font_glyphs[][BA_GLYPH_W] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ' ' */
    { 0x14, 0x7F, 0x14, 0x7F, 0x14 }, /* '#' */
    { 0x14, 0x08, 0x3E, 0x08, 0x14 }, /* '*' */
    { 0x08, 0x08, 0x3E, 0x08, 0x08 }, /* '+' */
    { 0x02, 0x01, 0x51, 0x09, 0x06 }, /* '?' */
    { 0x20, 0x10, 0x08, 0x04, 0x02 }, /* '/' */
    { 0x00, 0x41, 0x22, 0x14, 0x08 }, /* '>' */
    { 0x40, 0x40, 0x40, 0x40, 0x40 }, /* '_' */
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, /* '0' */
    { 0x00, 0x42, 0x7F, 0x40, 0x00 }, /* '1' */
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, /* '2' */
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, /* '3' */
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, /* '4' */
    { 0x27, 0x45, 0x45, 0x45, 0x39 }, /* '5' */
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, /* '6' */
    { 0x01, 0x71, 0x09, 0x05, 0x03 }, /* '7' */
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, /* '8' */
    { 0x06, 0x49, 0x49, 0x29, 0x1E }, /* '9' */
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, /* 'A' */
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, /* 'B' */
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, /* 'C' */
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, /* 'D' */
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, /* 'E' */
    { 0x7F, 0x09, 0x09, 0x09, 0x01 }, /* 'F' */
    { 0x3E, 0x41, 0x49, 0x49, 0x7A }, /* 'G' */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, /* 'H' */
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, /* 'I' */
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, /* 'L' */
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F }, /* 'M' */
    { 0x7F, 0x04, 0x08, 0x10, 0x7F }, /* 'N' */
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, /* 'P' */
    { 0x7F, 0x09, 0x19, 0x29, 0x46 }, /* 'R' */
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, /* 'S' */
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, /* 'T' */
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, /* 'U' */
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, /* 'V' */
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, /* 'X' */
    { 0x07, 0x08, 0x70, 0x08, 0x07 }, /* 'Y' */
    { 0x61, 0x51, 0x49, 0x45, 0x43 }, /* 'Z' */
};

#define BA_FONT_GLYPHS ((int) (sizeof(ba_font_glyphs) / sizeof(ba_font_glyphs[0])))

/* Un typo en la tabla (una letra de mas o de menos) no puede pasar en silencio:
   la tabla de caracteres y la de glifos tienen que tener el mismo largo. */
_Static_assert(sizeof(ba_font_chars) - 1 == (size_t) BA_FONT_GLYPHS,
               "ba_font_chars y ba_font_glyphs tienen distinto largo");

/* ---------------------------------------------------------------------------
 * Modelo
 * --------------------------------------------------------------------------- */
const char *ba_alphabet(void) {
    return BA_ALPHABET;
}

const char *ba_logo_text(void) {
    return BA_LOGO_TEXT;
}

const char *ba_header_text(void) {
    return BA_HEADER_TEXT;
}

const char *ba_subtitle_text(void) {
    return BA_SUBTITLE_TEXT;
}

uint16_t ba_logo_lock_ms(int pos) {
    /* Marcas del contrato: 250/375/500/625/750 ms desde el primer cuadro. */
    static const uint16_t marks[BA_LOGO_LEN] = { 250, 375, 500, 625, 750 };
    int                   i                  = pos < 0 ? 0 : (pos >= BA_LOGO_LEN ? BA_LOGO_LEN - 1 : pos);

    return marks[i];
}

char ba_logo_final_char(int pos) {
    int i = pos < 0 ? 0 : (pos >= BA_LOGO_LEN ? BA_LOGO_LEN - 1 : pos);

    return BA_LOGO_TEXT[i];
}

const char *ba_footer_text(const ba_model_t *m) {
    return (m != NULL && m->delayed) ? BA_DELAYED_TEXT : BA_FOOTER_TEXT;
}

/* Caracter provisional por posicion. Deterministico y sin PRNG (contrato):
 * alphabet[(frame_index * 7 + position * 11) % alphabet_length]. */
static char ba_scramble_char(uint32_t frame_index, int pos) {
    size_t len = strlen(BA_ALPHABET);

    return BA_ALPHABET[(size_t) (((uint64_t) frame_index * 7u + (uint64_t) pos * 11u) % len)];
}

void ba_model_at(uint32_t frame_index, uint64_t elapsed_ms, ba_model_t *out) {
    int pos;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));

    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        uint16_t lock = ba_logo_lock_ms(pos);

        out->logo_locked[pos] = elapsed_ms >= (uint64_t) lock;
        out->logo[pos] = out->logo_locked[pos] ? ba_logo_final_char(pos) : ba_scramble_char(frame_index, pos);
    }

    /* Antes de 875 ms el cursor queda dibujado (el pie literal lo trae); desde
     * ahi el '_' parpadea 500 ms encendido / 500 ms apagado. */
    if (elapsed_ms < (uint64_t) BA_LOGO_FULL_MS) {
        out->cursor_visible = true;
    } else {
        out->cursor_visible = (((elapsed_ms - BA_LOGO_FULL_MS) / BA_CURSOR_MS) % 2u) == 0u;
    }

    out->delayed = elapsed_ms >= (uint64_t) BA_TIMEOUT_MS;
}

/* ---------------------------------------------------------------------------
 * Fuente
 * --------------------------------------------------------------------------- */
const uint8_t *ba_glyph(char c) {
    int i;

    for (i = 0; i < BA_FONT_GLYPHS; i++) {
        if (ba_font_chars[i] == c) {
            return ba_font_glyphs[i];
        }
    }

    return NULL;
}

bool ba_text_supported(const char *text) {
    size_t i;

    if (text == NULL) {
        return false;
    }

    for (i = 0; text[i] != '\0'; i++) {
        if (ba_glyph(text[i]) == NULL) {
            return false;
        }
    }

    return true;
}

/* ---------------------------------------------------------------------------
 * Layout
 * --------------------------------------------------------------------------- */
int32_t ba_text_width(const char *text, int32_t scale) {
    size_t n;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    n = strlen(text);

    /* n glifos de 5 unidades + n-1 separaciones de 1 unidad. */
    return scale * (int32_t) (BA_GLYPH_ADV * n - 1);
}

int32_t ba_center_x(const ba_rect_t *r) {
    return (r->x1 + r->x2 + 1) / 2;
}

/* Columna izquierda del texto centrado en el ancho logico. */
static int32_t ba_text_x(const char *text, int32_t scale) {
    return BA_LOGICAL_W / 2 - ba_text_width(text, scale) / 2;
}

ba_rect_t ba_text_box(const char *text, int32_t scale, int32_t center_x, int32_t top_y) {
    ba_rect_t box;
    int32_t   w = ba_text_width(text, scale);
    int32_t   h = scale * BA_GLYPH_H;

    box.x1 = center_x - w / 2;
    box.y1 = top_y;
    box.x2 = box.x1 + w - 1;
    box.y2 = box.y1 + h - 1;

    return box;
}

ba_rect_t ba_logo_box(void) {
    ba_rect_t box = ba_text_box(BA_LOGO_TEXT, BA_LOGO_SCALE, BA_LOGICAL_W / 2, BA_LOGO_BOX_Y1);

    /* La caja del contrato es 168..254 (mas alta que el glifo): el glifo va
     * centrado adentro (ba_logo_glyph_y). */
    box.y2 = BA_LOGO_BOX_Y2;

    return box;
}

/* Fila superior del glifo del logo: centrado vertical en la caja del contrato. */
static int32_t ba_logo_glyph_y(void) {
    int32_t box_h = BA_LOGO_BOX_Y2 - BA_LOGO_BOX_Y1 + 1;
    int32_t glyph_h = BA_GLYPH_H * BA_LOGO_SCALE;

    return BA_LOGO_BOX_Y1 + (box_h - glyph_h) / 2;
}

ba_rect_t ba_logo_cell(int pos) {
    ba_rect_t box   = ba_logo_box();
    int32_t   step  = BA_GLYPH_ADV * BA_LOGO_SCALE;
    int32_t   i     = pos < 0 ? 0 : (pos >= BA_LOGO_LEN ? BA_LOGO_LEN - 1 : pos);

    box.x1 = box.x1 + i * step;
    box.x2 = box.x1 + BA_GLYPH_W * BA_LOGO_SCALE - 1;
    box.y1 = ba_logo_glyph_y();
    box.y2 = box.y1 + BA_GLYPH_H * BA_LOGO_SCALE - 1;

    return box;
}

ba_rect_t ba_header_box(void) {
    return ba_text_box(BA_HEADER_TEXT, BA_TEXT_SCALE, BA_LOGICAL_W / 2, BA_HEADER_BASELINE - BA_GLYPH_H * BA_TEXT_SCALE);
}

ba_rect_t ba_subtitle_box(void) {
    int32_t h = BA_GLYPH_H * BA_TEXT_SCALE;

    return ba_text_box(BA_SUBTITLE_TEXT, BA_TEXT_SCALE, BA_LOGICAL_W / 2, BA_SUBTITLE_CY - h / 2);
}

/* El pie puede mostrar dos textos: la caja es la union de los dos, para que el
 * cambio a STARTUP DELAYED y el parpadeo del cursor quepan sin repintar de mas. */
ba_rect_t ba_footer_box(void) {
    ba_rect_t a = ba_text_box(BA_FOOTER_TEXT, BA_TEXT_SCALE, BA_LOGICAL_W / 2, BA_FOOTER_CY - BA_GLYPH_H * BA_TEXT_SCALE / 2);
    ba_rect_t b = ba_text_box(BA_DELAYED_TEXT, BA_TEXT_SCALE, BA_LOGICAL_W / 2, BA_FOOTER_CY - BA_GLYPH_H * BA_TEXT_SCALE / 2);
    ba_rect_t box;

    box.x1 = a.x1 < b.x1 ? a.x1 : b.x1;
    box.y1 = a.y1 < b.y1 ? a.y1 : b.y1;
    box.x2 = a.x2 > b.x2 ? a.x2 : b.x2;
    box.y2 = a.y2 > b.y2 ? a.y2 : b.y2;

    return box;
}

ba_rect_t ba_cursor_cell(void) {
    /* Ultima celda del pie INITIALIZING_ (el '_' que parpadea): se calcula desde
     * el texto, no desde la union de las dos variantes del pie. */
    ba_rect_t box  = ba_text_box(BA_FOOTER_TEXT, BA_TEXT_SCALE, BA_LOGICAL_W / 2, BA_FOOTER_CY - BA_GLYPH_H * BA_TEXT_SCALE / 2);
    int32_t   len  = (int32_t) strlen(BA_FOOTER_TEXT);
    int32_t   step = BA_GLYPH_ADV * BA_TEXT_SCALE;
    int32_t   cell_w = BA_GLYPH_W * BA_TEXT_SCALE;

    box.x1 = box.x1 + (len - 1) * step;
    box.x2 = box.x1 + cell_w - 1;

    return box;
}

ba_rect_t ba_screen_box(void) {
    ba_rect_t box;

    box.x1 = 0;
    box.y1 = 0;
    box.x2 = BA_LOGICAL_W - 1;
    box.y2 = BA_LOGICAL_H - 1;

    return box;
}

bool ba_rect_valid(const ba_rect_t *r) {
    if (r == NULL) {
        return false;
    }

    return r->x1 <= r->x2 && r->y1 <= r->y2;
}

bool ba_rect_inside(const ba_rect_t *inner, const ba_rect_t *outer) {
    if (inner == NULL || outer == NULL) {
        return false;
    }

    return inner->x1 >= outer->x1 && inner->y1 >= outer->y1 && inner->x2 <= outer->x2 && inner->y2 <= outer->y2;
}

/* ---------------------------------------------------------------------------
 * Raster
 * --------------------------------------------------------------------------- */
static void ba_fill(ba_surface_t *s, const ba_rect_t *r, uint16_t color) {
    int32_t x, y;

    if (s == NULL || s->px == NULL || !ba_rect_valid(r)) {
        return;
    }

    for (y = r->y1; y <= r->y2; y++) {
        if (y < 0 || y >= s->h) {
            continue;
        }

        for (x = r->x1; x <= r->x2; x++) {
            if (x < 0 || x >= s->w) {
                continue;
            }

            s->px[(size_t) y * (size_t) s->w + (size_t) x] = color;
        }
    }
}

static void ba_draw_glyph(ba_surface_t *s, char c, int32_t scale, int32_t x, int32_t y, uint16_t color) {
    const uint8_t *g = ba_glyph(c);
    int32_t        col, row;

    if (g == NULL) {
        return;
    }

    for (col = 0; col < BA_GLYPH_W; col++) {
        for (row = 0; row < BA_GLYPH_H; row++) {
            if ((g[col] >> row) & 0x01u) {
                ba_rect_t px;

                px.x1 = x + col * scale;
                px.y1 = y + row * scale;
                px.x2 = px.x1 + scale - 1;
                px.y2 = px.y1 + scale - 1;
                ba_fill(s, &px, color);
            }
        }
    }
}

/* Dibuja los primeros `count` caracteres del texto (count < 0 = todos). */
static void ba_draw_text_n(ba_surface_t *s, const char *text, int32_t count, int32_t scale, int32_t x,
                           int32_t y, uint16_t color) {
    int32_t i;

    if (text == NULL) {
        return;
    }

    for (i = 0; text[i] != '\0'; i++) {
        if (count >= 0 && i >= count) {
            return;
        }

        ba_draw_glyph(s, text[i], scale, x + i * BA_GLYPH_ADV * scale, y, color);
    }
}

static void ba_draw_footer(ba_surface_t *s, const ba_model_t *m) {
    const char *text  = ba_footer_text(m);
    int32_t     count = (int32_t) strlen(text);

    /* Cursor apagado: se borra SOLO la ultima celda del pie (el resto del texto
     * ya esta pintado en el fondo) y la posicion no cambia. */
    if (!m->delayed && !m->cursor_visible) {
        count--;
    }

    ba_draw_text_n(s, text, count, BA_TEXT_SCALE, ba_text_x(text, BA_TEXT_SCALE),
                   BA_FOOTER_CY - BA_GLYPH_H * BA_TEXT_SCALE / 2, BA_COLOR_PRIMARY);
}

static void ba_draw_logo(ba_surface_t *s, const ba_model_t *m) {
    int32_t pos;

    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        int32_t cell_x = ba_logo_cell(pos).x1;

        ba_draw_glyph(s, m->logo[pos], BA_LOGO_SCALE, cell_x, ba_logo_glyph_y(), BA_COLOR_PRIMARY);
    }
}

static void ba_push(ba_dirty_t *dirty, const ba_rect_t *r) {
    if (dirty == NULL || dirty->count >= BA_MAX_DIRTY) {
        return;
    }

    dirty->rect[dirty->count++] = *r;
}

void ba_render(ba_surface_t *s, const ba_model_t *m, const ba_model_t *prev, bool full, ba_dirty_t *dirty) {
    ba_rect_t screen = ba_screen_box();
    int32_t   pos;

    if (dirty != NULL) {
        dirty->count = 0;
    }

    if (s == NULL || s->px == NULL || m == NULL) {
        return;
    }

    if (full || prev == NULL) {
        ba_rect_t box;

        /* Fondo completo: SOLO en el primer cuadro. */
        ba_fill(s, &screen, BA_COLOR_BG);

        box = ba_header_box();
        ba_draw_text_n(s, BA_HEADER_TEXT, -1, BA_TEXT_SCALE, box.x1, box.y1, BA_COLOR_SECONDARY);

        box = ba_subtitle_box();
        ba_draw_text_n(s, BA_SUBTITLE_TEXT, -1, BA_TEXT_SCALE, box.x1, box.y1, BA_COLOR_SECONDARY);

        ba_draw_footer(s, m);
        ba_draw_logo(s, m);

        ba_push(dirty, &screen);

        return;
    }

    /* Cuadros siguientes: solo las cajas del logo/cursor/pie que cambiaron,
     * borrando al color de fondo antes de reimprimir. */
    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        if (m->logo[pos] != prev->logo[pos] || m->logo_locked[pos] != prev->logo_locked[pos]) {
            ba_rect_t cell = ba_logo_cell(pos);

            ba_fill(s, &cell, BA_COLOR_BG);
            ba_draw_glyph(s, m->logo[pos], BA_LOGO_SCALE, cell.x1, ba_logo_glyph_y(), BA_COLOR_PRIMARY);
            ba_push(dirty, &cell);
        }
    }

    if (m->delayed != prev->delayed) {
        ba_rect_t box = ba_footer_box();

        ba_fill(s, &box, BA_COLOR_BG);
        ba_draw_footer(s, m);
        ba_push(dirty, &box);
    } else if (m->cursor_visible != prev->cursor_visible) {
        ba_rect_t cell = ba_cursor_cell();

        ba_fill(s, &cell, BA_COLOR_BG);
        ba_draw_footer(s, m);
        ba_push(dirty, &cell);
    }
}

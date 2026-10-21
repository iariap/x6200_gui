/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Tests de host del renderer del arranque neo-retro (issue #192, seccion 7):
 *  modelo de animacion (frames/tiempos/cursor/30 s), layout y centrado, texto
 *  exacto, cajas y snapshots reproducibles. Todo en memoria, sin hardware.
 */
#include "renderer.h"
#include "test_util.h"

#include <stdlib.h>

static uint16_t *surface_alloc(void) {
    uint16_t *px = malloc((size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t));

    if (px == NULL) {
        printf("FAIL: sin memoria para el surface\n");
        exit(1);
    }

    memset(px, 0xA5, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t));

    return px;
}

static ba_surface_t surface_wrap(uint16_t *px) {
    ba_surface_t s;

    s.px = px;
    s.w  = BA_LOGICAL_W;
    s.h  = BA_LOGICAL_H;

    return s;
}

/* Reproduce la animacion desde el cuadro 0 hasta `last` (inclusive) y devuelve
 * el surface acumulado: es el mismo camino que sigue src/bootanim/main.c. */
static void replay(ba_surface_t *s, uint32_t last) {
    ba_model_t prev;
    bool       have_prev = false;
    uint32_t   frame;

    for (frame = 0; frame <= last; frame++) {
        ba_model_t m;
        ba_dirty_t dirty;

        ba_model_at(frame, (uint64_t) frame * BA_FRAME_MS, &m);
        ba_render(s, &m, have_prev ? &prev : NULL, !have_prev, &dirty);
        prev      = m;
        have_prev = true;

        if (m.delayed) {
            break;
        }
    }
}

/* Pixeles que ocupan los glifos de un texto a la escala dada (oraculo del area
 * pintada: 1 bit del glifo = escala^2 pixeles). */
static int expected_painted(const char *text, int scale) {
    int total = 0;
    int i;

    for (i = 0; text[i] != '\0'; i++) {
        const uint8_t *g = ba_glyph(text[i]);
        int            col, row;

        if (g == NULL) {
            continue;
        }

        for (col = 0; col < BA_GLYPH_W; col++) {
            for (row = 0; row < BA_GLYPH_H; row++) {
                if ((g[col] >> row) & 1u) {
                    total += scale * scale;
                }
            }
        }
    }

    return total;
}

/* Pixeles pintados con un color dentro de un rectangulo logico. */
static int count_color(const ba_surface_t *s, const ba_rect_t *r, uint16_t color) {    int x, y, n = 0;

    for (y = r->y1; y <= r->y2; y++) {
        for (x = r->x1; x <= r->x2; x++) {
            if (s->px[(size_t) y * (size_t) s->w + (size_t) x] == color) {
                n++;
            }
        }
    }

    return n;
}

static void test_model(void) {
    ba_model_t m;
    int        pos;

    /* Cuadro 0: las cinco posiciones muestran el alfabeto fijo, con la formula
     * determinista alphabet[(frame*7 + pos*11) % len]. */
    ba_model_at(0, 0, &m);

    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        const char *alphabet = ba_alphabet();
        size_t      len      = strlen(alphabet);
        char        esperado = alphabet[(size_t) ((0u * 7u + (uint64_t) pos * 11u) % len)];

        BA_CHECK_EQ_INT(m.logo[pos], esperado);
        BA_CHECK(m.logo_locked[pos] == false);
    }

    BA_CHECK(m.cursor_visible == true);
    BA_CHECK(m.delayed == false);

    /* El alfabeto es el del contrato, exacto. */
    BA_CHECK_STREQ(ba_alphabet(), "#*+?0123456789ABCDEF");

    /* Marcas de bloqueo 250/375/500/625/750 ms y caracteres finales X6200. */
    {
        static const uint16_t marks[BA_LOGO_LEN] = { 250, 375, 500, 625, 750 };
        static const char     final[BA_LOGO_LEN] = { 'X', '6', '2', '0', '0' };

        for (pos = 0; pos < BA_LOGO_LEN; pos++) {
            uint64_t antes   = (uint64_t) marks[pos] - 1u;
            uint32_t frame_a = (uint32_t) (antes / BA_FRAME_MS);
            size_t   len     = strlen(ba_alphabet());
            char     scrambled =
                ba_alphabet()[(size_t) (((uint64_t) frame_a * 7u + (uint64_t) pos * 11u) % len)];

            BA_CHECK_EQ_INT(ba_logo_lock_ms(pos), marks[pos]);
            BA_CHECK_EQ_INT(ba_logo_final_char(pos), final[pos]);

            /* Un milisegundo antes de la marca sigue descifrando: el caracter es
             * el provisional de esa posicion (formula del contrato). */
            ba_model_at(frame_a, antes, &m);
            BA_CHECK_MSG(m.logo_locked[pos] == false, "pos=%d", pos);
            BA_CHECK_MSG(m.logo[pos] == scrambled, "pos=%d: %c != %c", pos, m.logo[pos], scrambled);

            /* En la marca queda fijo en el caracter final. */
            ba_model_at((uint32_t) (marks[pos] / BA_FRAME_MS), (uint64_t) marks[pos], &m);
            BA_CHECK_MSG(m.logo_locked[pos] == true, "pos=%d", pos);
            BA_CHECK_EQ_INT(m.logo[pos], final[pos]);
        }
    }

    /* Cursor: 500 ms encendido / 500 ms apagado desde 875 ms; antes, encendido. */
    ba_model_at(6, 750, &m);
    BA_CHECK(m.cursor_visible == true); /* previo al logo completo */
    ba_model_at(7, 875, &m);
    BA_CHECK(m.cursor_visible == true);
    ba_model_at(10, 1375, &m);
    BA_CHECK(m.cursor_visible == false);
    ba_model_at(14, 1875, &m);
    BA_CHECK(m.cursor_visible == true);
    ba_model_at(18, 2375, &m);
    BA_CHECK(m.cursor_visible == false);

    /* 30 s: pie STARTUP DELAYED, logo completo y cursor dibujado (se congela). */
    ba_model_at(240, 30000, &m);
    BA_CHECK(m.delayed == true);
    BA_CHECK_STREQ(ba_footer_text(&m), "> STARTUP DELAYED_");

    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        BA_CHECK_EQ_INT(m.logo[pos], ba_logo_text()[pos]);
        BA_CHECK(m.logo_locked[pos] == true);
    }

    /* Textos del contrato, exactos y en ingles. */
    BA_CHECK_STREQ(ba_logo_text(), "X6200");
    BA_CHECK_STREQ(ba_header_text(), "X6200 / SYSTEM STARTUP");
    BA_CHECK_STREQ(ba_subtitle_text(), "HF TRANSCEIVER");
    BA_CHECK_STREQ(ba_footer_text(NULL), "> INITIALIZING_");

    /* Todos los textos usan glifos que existen: no hay caracteres sin dibujar
     * (y por lo tanto no puede aparecer un '%' ni un indicador ficticio). */
    BA_CHECK(ba_text_supported(ba_logo_text()));
    BA_CHECK(ba_text_supported(ba_header_text()));
    BA_CHECK(ba_text_supported(ba_subtitle_text()));
    BA_CHECK(ba_text_supported("> INITIALIZING_"));
    BA_CHECK(ba_text_supported("> STARTUP DELAYED_"));
    BA_CHECK(ba_text_supported(ba_alphabet()));
    BA_CHECK(ba_text_supported(" "));
    BA_CHECK(ba_glyph('%') == NULL);

    /* Cuadro 7 (875 ms): el logo ya esta completo y fijo. */
    ba_model_at(7, 875, &m);
    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        BA_CHECK_EQ_INT(m.logo[pos], ba_logo_text()[pos]);
    }
}

static void test_layout(void) {
    ba_rect_t logo = ba_logo_box();
    ba_rect_t cell;
    ba_rect_t header = ba_header_box();
    ba_rect_t sub    = ba_subtitle_box();
    ba_rect_t foot   = ba_footer_box();
    ba_rect_t cursor = ba_cursor_cell();
    ba_rect_t screen = ba_screen_box();
    int       pos;

    /* Logo: 5 glifos de 50 px con separacion 10 -> 290 px, centrado en x=400. */
    BA_CHECK_EQ_INT(logo.x1, 255);
    BA_CHECK_EQ_INT(logo.x2, 544);
    BA_CHECK_EQ_INT(logo.x2 - logo.x1 + 1, 290);
    BA_CHECK_EQ_INT(ba_center_x(&logo), 400);

    /* Caja del contrato: y=168..254, glifo de 70 px centrado adentro. */
    BA_CHECK_EQ_INT(logo.y1, 168);
    BA_CHECK_EQ_INT(logo.y2, 254);

    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        cell = ba_logo_cell(pos);
        BA_CHECK_EQ_INT(cell.x2 - cell.x1 + 1, 50);
        BA_CHECK_EQ_INT(cell.y2 - cell.y1 + 1, 70);
        BA_CHECK_EQ_INT(cell.x1, 255 + pos * 60);
        BA_CHECK(cell.y1 == 176 && cell.y2 == 245); /* 70 px centrados en 168..254 */
        BA_CHECK(ba_rect_inside(&cell, &logo));
    }

    /* Encabezado: baseline 70, escala 3 (21 px de alto), centrado. */
    BA_CHECK_EQ_INT(header.y1, 49);
    BA_CHECK_EQ_INT(header.y2, 69);
    BA_CHECK_EQ_INT(ba_center_x(&header), 400);
    BA_CHECK(ba_rect_inside(&header, &screen));

    /* Subtitulo: centrado en y=297. */
    BA_CHECK_EQ_INT((sub.y1 + sub.y2 + 1) / 2, 297);
    BA_CHECK_EQ_INT(ba_center_x(&sub), 400);
    BA_CHECK(ba_rect_inside(&sub, &screen));

    /* Pie: centrado en y=390 y la caja cubre las dos variantes (INITIALIZING y
     * STARTUP DELAYED), que no deben desbordar 800x480. */
    BA_CHECK_EQ_INT((foot.y1 + foot.y2 + 1) / 2, 390);
    BA_CHECK(ba_rect_inside(&foot, &screen));
    {
        ba_rect_t foot_ini = ba_text_box("> INITIALIZING_", BA_TEXT_SCALE, 400, foot.y1);
        ba_rect_t foot_del = ba_text_box("> STARTUP DELAYED_", BA_TEXT_SCALE, 400, foot.y1);

        BA_CHECK(ba_rect_inside(&foot_ini, &screen));
        BA_CHECK(ba_rect_inside(&foot_del, &screen));
    }
    BA_CHECK_EQ_INT(ba_text_width("> INITIALIZING_", BA_TEXT_SCALE), 267);
    BA_CHECK_EQ_INT(ba_text_width("> STARTUP DELAYED_", BA_TEXT_SCALE), 321);

    /* Celda del cursor: ultimo glifo del pie INITIALIZING_. */
    {
        ba_rect_t foot_ini = ba_text_box("> INITIALIZING_", BA_TEXT_SCALE, 400, cursor.y1);

        BA_CHECK_EQ_INT(cursor.x2 - cursor.x1 + 1, 15);
        BA_CHECK_EQ_INT(cursor.x1, foot_ini.x2 - 14);
    }
    BA_CHECK(ba_rect_inside(&cursor, &foot));

    /* Anchura: n glifos de 5*escala + n-1 separaciones de escala. */
    BA_CHECK_EQ_INT(ba_text_width("X6200", BA_LOGO_SCALE), 290);
    BA_CHECK_EQ_INT(ba_text_width("X6200 / SYSTEM STARTUP", BA_TEXT_SCALE), 393);
    BA_CHECK_EQ_INT(ba_text_width("HF TRANSCEIVER", BA_TEXT_SCALE), 249);
    BA_CHECK_EQ_INT(ba_text_width("", BA_TEXT_SCALE), 0);

    /* Nada de cajas invalidas. */
    BA_CHECK(ba_rect_valid(&screen));
    BA_CHECK(ba_rect_inside(&logo, &screen));
}

static void test_raster(void) {
    uint16_t    *px  = surface_alloc();
    ba_surface_t s   = surface_wrap(px);
    ba_model_t   m0;
    ba_model_t   m7;
    ba_dirty_t   dirty;
    int          pos;

    /* Cuadro 0 (completo): fondo en toda la pantalla y dirty = pantalla entera. */
    ba_model_at(0, 0, &m0);
    ba_render(&s, &m0, NULL, true, &dirty);

    BA_CHECK_EQ_INT(dirty.count, 1);
    BA_CHECK_EQ_INT(dirty.rect[0].x1, 0);
    BA_CHECK_EQ_INT(dirty.rect[0].y1, 0);
    BA_CHECK_EQ_INT(dirty.rect[0].x2, BA_LOGICAL_W - 1);
    BA_CHECK_EQ_INT(dirty.rect[0].y2, BA_LOGICAL_H - 1);

    {
        int x, y, bg = 0, ajeno = 0;

        for (y = 0; y < BA_LOGICAL_H; y++) {
            for (x = 0; x < BA_LOGICAL_W; x++) {
                uint16_t v = s.px[(size_t) y * BA_LOGICAL_W + (size_t) x];

                if (v == BA_COLOR_BG) {
                    bg++;
                }

                if (v != BA_COLOR_BG && v != BA_COLOR_PRIMARY && v != BA_COLOR_SECONDARY) {
                    ajeno++;
                }
            }
        }

        /* El primer cuadro pinta TODO el fondo (ningun pixel quedo con el patron
         * A5) y los unicos colores son los tres del contrato: nada de gradientes
         * ni colores fuera de la paleta. */
        BA_CHECK_EQ_INT(ajeno, 0);
        BA_CHECK(bg > 370000);

        /* El area pintada es exactamente la de los glifos de los cuatro textos,
         * al tamano del contrato: oraculo independiente del layout. */
        {
            int esperados = expected_painted(ba_header_text(), BA_TEXT_SCALE) +
                            expected_painted(ba_subtitle_text(), BA_TEXT_SCALE) +
                            expected_painted("> INITIALIZING_", BA_TEXT_SCALE);

            for (pos = 0; pos < BA_LOGO_LEN; pos++) {
                char logo_cell_text[2];

                logo_cell_text[0] = m0.logo[pos];
                logo_cell_text[1] = '\0';
                esperados += expected_painted(logo_cell_text, BA_LOGO_SCALE);
            }

            BA_CHECK_EQ_INT(BA_LOGICAL_W * BA_LOGICAL_H - bg, esperados);
        }
    }

    /* El logo del cuadro 0 usa solo glifos del alfabeto (colores primarios). */
    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        BA_CHECK(count_color(&s, &(ba_rect_t) { ba_logo_cell(pos).x1, ba_logo_cell(pos).y1,
                                                ba_logo_cell(pos).x2, ba_logo_cell(pos).y2 },
                            BA_COLOR_PRIMARY) > 0);
    }

    /* Cuadro con todo fijado: "X6200" dibujado glifo por glifo en las cajas del
     * contrato; se compara celda por celda contra la caja esperada. */
    ba_model_at(7, 875, &m7);
    ba_render(&s, &m7, &m0, false, &dirty);
    BA_CHECK(dirty.count > 0);
    BA_CHECK(dirty.count <= BA_MAX_DIRTY);

    {
        int todos_locked = 1;

        for (pos = 0; pos < BA_LOGO_LEN; pos++) {
            if (m7.logo[pos] != "X6200"[pos]) {
                todos_locked = 0;
            }
        }

        BA_CHECK(todos_locked == 1);
    }

    /* Comparacion del raster contra un oraculo independiente: la celda pintada
     * tiene exactamente los pixeles del glifo escalado a 10. */
    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        ba_rect_t      cell = ba_logo_cell(pos);
        const uint8_t *g    = ba_glyph("X6200"[pos]);
        int            col, row, esperados = 0;

        BA_CHECK(g != NULL);

        for (col = 0; col < BA_GLYPH_W; col++) {
            for (row = 0; row < BA_GLYPH_H; row++) {
                if ((g[col] >> row) & 1u) {
                    esperados += BA_LOGO_SCALE * BA_LOGO_SCALE;
                }
            }
        }

        BA_CHECK_EQ_INT(count_color(&s, &cell, BA_COLOR_PRIMARY), esperados);
    }

    /* Cuadros siguientes: solo cambian las cinco cajas del logo. */
    {
        ba_model_t m8;
        ba_dirty_t d8;

        ba_model_at(8, 1000, &m8);
        ba_render(&s, &m8, &m7, false, &d8);
        BA_CHECK_EQ_INT(d8.count, 0); /* el logo ya no cambia: nada que repintar */

        /* Cursor apagado -> se repinta SOLO su celda. */
        {
            ba_model_t m10;
            ba_dirty_t d10;

            ba_model_at(10, 1375, &m10);
            BA_CHECK(m10.cursor_visible == false);
            ba_render(&s, &m10, &m8, false, &d10);
            BA_CHECK_EQ_INT(d10.count, 1);
            BA_CHECK_EQ_INT(d10.rect[0].x1, ba_cursor_cell().x1);
            BA_CHECK_EQ_INT(d10.rect[0].y1, ba_cursor_cell().y1);
            BA_CHECK_EQ_INT(d10.rect[0].x2, ba_cursor_cell().x2);
            BA_CHECK_EQ_INT(d10.rect[0].y2, ba_cursor_cell().y2);

            /* La celda quedo en fondo y el resto del pie intacto. */
            {
                ba_rect_t cell   = ba_cursor_cell();
                ba_rect_t footer = ba_footer_box();

                BA_CHECK_EQ_INT(count_color(&s, &cell, BA_COLOR_PRIMARY), 0);
                BA_CHECK(count_color(&s, &footer, BA_COLOR_PRIMARY) > 0);
            }

            /* Cursor encendido otra vez: vuelve el '_'. */
            {
                ba_model_t m14;
                ba_dirty_t d14;
                ba_rect_t  cell = ba_cursor_cell();

                ba_model_at(14, 1875, &m14);
                ba_render(&s, &m14, &m10, false, &d14);
                BA_CHECK_EQ_INT(d14.count, 1);
                BA_CHECK(count_color(&s, &cell, BA_COLOR_PRIMARY) > 0);
            }
        }
    }

    /* Determinismo: dos corridas de la animacion completa dan bytes identicos. */
    {
        uint16_t    *px_b = surface_alloc();
        ba_surface_t s_b  = surface_wrap(px_b);

        replay(&s, 239);
        replay(&s_b, 239);
        BA_CHECK(memcmp(px, px_b, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t)) == 0);
        free(px_b);
    }

    /* 30 s: el pie pasa a STARTUP DELAYED_ en su caja, sin tocar el logo. */
    {
        ba_model_t m239, m240;
        ba_dirty_t d240;

        ba_model_at(239, 29875, &m239);
        ba_model_at(240, 30000, &m240);
        ba_render(&s, &m240, &m239, false, &d240);
        BA_CHECK_EQ_INT(d240.count, 1);
        BA_CHECK_EQ_INT(d240.rect[0].x1, ba_footer_box().x1);
        BA_CHECK_EQ_INT(d240.rect[0].y2, ba_footer_box().y2);
        BA_CHECK_STREQ(ba_footer_text(&m240), "> STARTUP DELAYED_");
    }

    free(px);
}

int main(void) {
    test_model();
    test_layout();
    test_raster();
    BA_TEST_SUMMARY("renderer_test");
}

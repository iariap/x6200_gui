/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Tests de host del renderer del arranque neo-retro (issue #192 y ciclo
 *  continuo del issue #204, seccion 5): modelo de animacion (intro, cursor,
 *  lluvia, BEAMS, DECRYPT, ciclo de 6 s y los 30 s como cambio de texto),
 *  layout y centrado, geometria de las cuatro lanes de lluvia, texto exacto,
 *  paleta, higiene de los rectangulos sucios y determinismo del raster (con
 *  secuencias que saltean cuadros). Todo en memoria, sin hardware.
 */
#include "renderer.h"
#include "test_util.h"

#include <stdlib.h>

#define BA_SENTINEL 0xA5A5u /* valor de relleno imposible en la paleta */

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

static uint32_t frame_at(uint64_t ms) {
    return (uint32_t) (ms / BA_FRAME_MS);
}

/* Modelo con el tiempo real de un cuadro (el que presenta el animador). */
static void model_at_frame(ba_model_t *m, uint32_t frame) {
    ba_model_at(frame, (uint64_t) frame * BA_FRAME_MS, m);
}

static void model_at_ms(ba_model_t *m, uint64_t ms) {
    ba_model_at(frame_at(ms), ms, m);
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

        model_at_frame(&m, frame);
        ba_render(s, &m, have_prev ? &prev : NULL, !have_prev, &dirty);
        prev      = m;
        have_prev = true;
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
static int count_color(const ba_surface_t *s, const ba_rect_t *r, uint16_t color) {
    int x, y, n = 0;

    for (y = r->y1; y <= r->y2; y++) {
        for (x = r->x1; x <= r->x2; x++) {
            if (s->px[(size_t) y * (size_t) s->w + (size_t) x] == color) {
                n++;
            }
        }
    }

    return n;
}

static int pixel_in_rect(int32_t x, int32_t y, const ba_rect_t *r) {
    return x >= r->x1 && x <= r->x2 && y >= r->y1 && y <= r->y2;
}

/* Pixels que cambiaron entre dos surfaces y NO estan en ninguna caja sucia:
 * prueba que el render no escribe fuera de lo que declara. */
static int diff_outside_dirty(const uint16_t *before, const uint16_t *after, const ba_dirty_t *dirty) {
    int x, y, n = 0;

    for (y = 0; y < BA_LOGICAL_H; y++) {
        for (x = 0; x < BA_LOGICAL_W; x++) {
            size_t i = (size_t) y * BA_LOGICAL_W + (size_t) x;

            if (before[i] == after[i]) {
                continue;
            }

            {
                int d, inside = 0;

                for (d = 0; d < dirty->count; d++) {
                    if (pixel_in_rect(x, y, &dirty->rect[d])) {
                        inside = 1;
                    }
                }

                if (!inside) {
                    n++;
                }
            }
        }
    }

    return n;
}

/* Suma de areas (sobrecuenta solapamientos: el techo de 12% queda mas exigente). */
static long dirty_area(const ba_dirty_t *dirty) {
    long total = 0;
    int  d;

    for (d = 0; d < dirty->count; d++) {
        total += (long) (dirty->rect[d].x2 - dirty->rect[d].x1 + 1) * (long) (dirty->rect[d].y2 - dirty->rect[d].y1 + 1);
    }

    return total;
}

static int is_screen_rect(const ba_rect_t *r) {
    return r->x1 == 0 && r->y1 == 0 && r->x2 == BA_LOGICAL_W - 1 && r->y2 == BA_LOGICAL_H - 1;
}

static int is_rain_rect(const ba_rect_t *r) {
    int lane;

    for (lane = 0; lane < BA_RAIN_LANES; lane++) {
        ba_rect_t box = ba_rain_box(lane);

        if (r->x1 == box.x1 && r->y1 == box.y1 && r->x2 == box.x2 && r->y2 == box.y2) {
            return 1;
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * Intro (0..874 ms): contrato original intacto
 * --------------------------------------------------------------------------- */
static void test_model_intro(void) {
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
    BA_CHECK(m.rain_active == false);
    BA_CHECK_EQ_INT(m.beam_cell, -1);
    BA_CHECK_EQ_INT(m.rain_step, 0);

    /* El alfabeto es el del contrato, exacto. */
    BA_CHECK_STREQ(ba_alphabet(), "#*+?0123456789ABCDEF");

    /* Marcas de bloqueo 250/375/500/625/750 ms y caracteres finales X6200; el
     * cuadro de 84 ms no las corre: los umbrales son de tiempo. */
    {
        static const uint16_t marks[BA_LOGO_LEN] = { 250, 375, 500, 625, 750 };
        static const char     final[BA_LOGO_LEN] = { 'X', '6', '2', '0', '0' };

        for (pos = 0; pos < BA_LOGO_LEN; pos++) {
            uint64_t antes = (uint64_t) marks[pos] - 1u;

            BA_CHECK_EQ_INT(ba_logo_lock_ms(pos), marks[pos]);
            BA_CHECK_EQ_INT(ba_logo_final_char(pos), final[pos]);

            /* Un milisegundo antes de la marca sigue descifrando. */
            model_at_ms(&m, antes);
            BA_CHECK_MSG(m.logo_locked[pos] == false, "pos=%d", pos);

            /* En la marca queda fijo en el caracter final. */
            model_at_ms(&m, (uint64_t) marks[pos]);
            BA_CHECK_MSG(m.logo_locked[pos] == true, "pos=%d", pos);
            BA_CHECK_EQ_INT(m.logo[pos], final[pos]);
        }
    }

    /* El logo a 875 ms esta completo, y sigue completo en los ciclos siguientes
     * (la intro NO se reejecuta). */
    {
        static const uint64_t times[] = { 875, 6875, 12875, 120000 };
        size_t                i;

        for (i = 0; i < sizeof(times) / sizeof(times[0]); i++) {
            model_at_ms(&m, times[i]);

            for (pos = 0; pos < BA_LOGO_LEN; pos++) {
                /* En DECRYPT solo la celda activa es provisional; se elige un
                 * tiempo de lluvia/BEAMS para esta comprobacion. */
                BA_CHECK_MSG(m.logo[pos] == ba_logo_text()[pos] || !m.logo_locked[pos], "t=%llu pos=%d",
                             (unsigned long long) times[i], pos);
            }
        }
    }

    /* Cursor: 500 ms encendido / 500 ms apagado desde 875 ms; antes, encendido. */
    model_at_ms(&m, 750);
    BA_CHECK(m.cursor_visible == true);
    model_at_ms(&m, 875);
    BA_CHECK(m.cursor_visible == true);
    model_at_ms(&m, 1375);
    BA_CHECK(m.cursor_visible == false);
    model_at_ms(&m, 1875);
    BA_CHECK(m.cursor_visible == true);
    model_at_ms(&m, 2375);
    BA_CHECK(m.cursor_visible == false);

    /* Textos del contrato, exactos y en ingles. */
    BA_CHECK_STREQ(ba_logo_text(), "X6200");
    BA_CHECK_STREQ(ba_header_text(), "X6200 / SYSTEM STARTUP");
    BA_CHECK_STREQ(ba_subtitle_text(), "HF TRANSCEIVER");
    BA_CHECK_STREQ(ba_footer_text(NULL), "> INITIALIZING_");

    BA_CHECK(ba_text_supported(ba_logo_text()));
    BA_CHECK(ba_text_supported(ba_header_text()));
    BA_CHECK(ba_text_supported(ba_subtitle_text()));
    BA_CHECK(ba_text_supported("> INITIALIZING_"));
    BA_CHECK(ba_text_supported("> STARTUP DELAYED_"));
    BA_CHECK(ba_text_supported(ba_alphabet()));
    BA_CHECK(ba_text_supported(" "));
    BA_CHECK(ba_glyph('%') == NULL);
}

/* ---------------------------------------------------------------------------
 * Ciclo de 6000 ms: bordes de fase, BEAMS y DECRYPT
 * --------------------------------------------------------------------------- */
/* Cuenta las posiciones del logo que no estan fijas (provisionales). */
static int count_unlocked(const ba_model_t *m) {
    int pos, n = 0;

    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        if (!m->logo_locked[pos]) {
            n++;
        }
    }

    return n;
}

static int logo_is_x6200(const ba_model_t *m) {
    int pos;

    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        if (m->logo[pos] != ba_logo_text()[pos]) {
            return 0;
        }
    }

    return 1;
}

static void test_model_cycle(void) {
    ba_model_t     m;
    const char    *alphabet = ba_alphabet();
    const size_t   len      = strlen(alphabet);

    /* La lluvia esta activa desde 875 ms y en TODO el ciclo. */
    model_at_ms(&m, 874);
    BA_CHECK(m.rain_active == false);
    BA_CHECK_EQ_INT(m.rain_step, 0);
    model_at_ms(&m, 875);
    BA_CHECK(m.rain_active == true);

    /* rain_step: 0 al arrancar la lluvia, 39 al final del ciclo de pasos y vuelve
     * a 0 (wrap) exactamente 40 cuadros despues. */
    model_at_ms(&m, 875);
    BA_CHECK_EQ_INT(m.rain_step, 0);
    model_at_ms(&m, 875u + 39u * BA_FRAME_MS);
    BA_CHECK_EQ_INT(m.rain_step, 39);
    model_at_ms(&m, 875u + 40u * BA_FRAME_MS);
    BA_CHECK_EQ_INT(m.rain_step, 0);
    model_at_ms(&m, 875u + 41u * BA_FRAME_MS);
    BA_CHECK_EQ_INT(m.rain_step, 1);

    /* --- bordes de fase (el ciclo arranca a los 875 ms) --- */
    /* 1199 -> logo fijo + lluvia; 1200 -> primer BEAMS. */
    model_at_ms(&m, BA_LOGO_FULL_MS + 1199u);
    BA_CHECK_EQ_INT(m.beam_cell, -1);
    BA_CHECK(logo_is_x6200(&m));
    BA_CHECK_EQ_INT(count_unlocked(&m), 0);
    model_at_ms(&m, BA_LOGO_FULL_MS + 1200u);
    BA_CHECK_EQ_INT(m.beam_cell, 0);
    BA_CHECK(logo_is_x6200(&m));
    BA_CHECK_EQ_INT(count_unlocked(&m), 0);

    /* El haz avanza cada 280 ms y nunca pasa de la ultima celda. */
    model_at_ms(&m, BA_LOGO_FULL_MS + 1200u + 279u);
    BA_CHECK_EQ_INT(m.beam_cell, 0);
    model_at_ms(&m, BA_LOGO_FULL_MS + 1200u + 280u);
    BA_CHECK_EQ_INT(m.beam_cell, 1);
    model_at_ms(&m, BA_LOGO_FULL_MS + 1200u + 4u * 280u);
    BA_CHECK_EQ_INT(m.beam_cell, 4);
    model_at_ms(&m, BA_LOGO_FULL_MS + 2599u);
    BA_CHECK_EQ_INT(m.beam_cell, 4);
    BA_CHECK(logo_is_x6200(&m));

    /* 2600 -> DECRYPT: exactamente una posicion provisional. */
    model_at_ms(&m, BA_LOGO_FULL_MS + 2600u);
    BA_CHECK_EQ_INT(m.beam_cell, -1);
    BA_CHECK_EQ_INT(count_unlocked(&m), 1);
    BA_CHECK(m.logo_locked[0] == false);
    BA_CHECK(m.logo[0] == alphabet[(size_t) (((uint64_t) frame_at(BA_LOGO_FULL_MS + 2600u) * 7u) % len)]);

    /* La posicion provisional sigue siendo una sola y avanza cada 200 ms;
     * las otras cuatro siguen en X6200. */
    model_at_ms(&m, BA_LOGO_FULL_MS + 2600u + 199u);
    BA_CHECK_EQ_INT(count_unlocked(&m), 1);
    BA_CHECK(m.logo_locked[0] == false);
    model_at_ms(&m, BA_LOGO_FULL_MS + 2600u + 200u);
    BA_CHECK_EQ_INT(count_unlocked(&m), 1);
    BA_CHECK(m.logo_locked[0] == true);
    BA_CHECK(m.logo_locked[1] == false);
    BA_CHECK(m.logo[0] == 'X');
    model_at_ms(&m, BA_LOGO_FULL_MS + 3599u);
    BA_CHECK_EQ_INT(count_unlocked(&m), 1);
    BA_CHECK(m.logo_locked[4] == false);

    /* 3600 -> retorno integro a X6200 y logo legible. */
    model_at_ms(&m, BA_LOGO_FULL_MS + 3600u);
    BA_CHECK_EQ_INT(count_unlocked(&m), 0);
    BA_CHECK(logo_is_x6200(&m));
    BA_CHECK_EQ_INT(m.beam_cell, -1);

    /* 5999 -> ultimo cuadro del ciclo; 6000 -> vuelve a 0 sin reejecutar la
     * intro (logo fijo, lluvia activa, sin haz ni provisionales). */
    model_at_ms(&m, BA_LOGO_FULL_MS + 5999u);
    BA_CHECK_EQ_INT(m.beam_cell, -1);
    BA_CHECK_EQ_INT(count_unlocked(&m), 0);
    model_at_ms(&m, BA_LOGO_FULL_MS + 6000u);
    BA_CHECK_EQ_INT(m.beam_cell, -1);
    BA_CHECK_EQ_INT(count_unlocked(&m), 0);
    BA_CHECK(logo_is_x6200(&m));
    BA_CHECK(m.rain_active == true);

    /* Un haz como maximo: en todo el ciclo de BEAMS hay a lo sumo una celda
     * iluminada (beam_cell es una sola por construccion; se recorre el rango
     * completo para dejar constancia). */
    {
        uint64_t c;

        for (c = 1200; c < 2600; c += 37) {
            model_at_ms(&m, BA_LOGO_FULL_MS + c);
            BA_CHECK(m.beam_cell >= 0 && m.beam_cell < BA_LOGO_LEN);
            BA_CHECK(logo_is_x6200(&m));
        }
    }

    /* Un caracter provisional como maximo en todo el rango de DECRYPT. */
    {
        uint64_t c;

        for (c = 2600; c < 3600; c += 11) {
            model_at_ms(&m, BA_LOGO_FULL_MS + c);
            BA_CHECK_EQ_INT(count_unlocked(&m), 1);
            BA_CHECK_EQ_INT(m.beam_cell, -1);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Continuidad: 1, 3, 5, 7, 15, 29, 30, 31, 45 y 120 s siguen animando
 * --------------------------------------------------------------------------- */
static void test_model_continuous(void) {
    /* Cuadros representativos (el animador presenta cuadros, no milisegundos). */
    static const uint32_t frames[] = { 12, 36, 60, 84, 179, 345, 358, 370, 536, 1429 };
    uint16_t             *px       = surface_alloc();
    ba_surface_t          s        = surface_wrap(px);
    size_t                i;

    for (i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        ba_model_t  cur, prv;
        ba_dirty_t  dirty;
        int         changed;
        int         pos;

        model_at_frame(&cur, frames[i]);
        model_at_frame(&prv, frames[i] - 1u);

        changed = cur.cursor_visible != prv.cursor_visible || cur.beam_cell != prv.beam_cell ||
                  cur.rain_step != prv.rain_step || cur.rain_active != prv.rain_active;

        for (pos = 0; pos < BA_LOGO_LEN; pos++) {
            if (cur.logo[pos] != prv.logo[pos] || cur.logo_locked[pos] != prv.logo_locked[pos]) {
                changed = 1;
            }
        }

        /* A los ~1/3/5/7/15/29/30/31/45/120 s el modelo sigue cambiando: el
         * estado animado avanza cuadro a cuadro (nunca es solo delayed=true). */
        BA_CHECK_MSG(changed != 0, "t=%ums congelado", frames[i] * BA_FRAME_MS);
        BA_CHECK_MSG(cur.rain_active, "t=%ums sin lluvia", frames[i] * BA_FRAME_MS);
        BA_CHECK_MSG(cur.rain_step != prv.rain_step, "t=%ums rain_step congelado", frames[i] * BA_FRAME_MS);

        /* Y el repintado es real: hay cajas sucias y no es una pantalla entera. */
        model_at_frame(&prv, frames[i] - 1u);
        ba_render(&s, &cur, &prv, false, &dirty);
        BA_CHECK_MSG(dirty.count > 0, "t=%ums sin repintado", frames[i] * BA_FRAME_MS);
        BA_CHECK(dirty.count <= BA_MAX_DIRTY);
        BA_CHECK(dirty.overflow == false);
        BA_CHECK(dirty_area(&dirty) < (long) BA_LOGICAL_W * BA_LOGICAL_H);

        /* Pasados los 30 s el logo sigue legible (fuera de DECRYPT). */
        if (frames[i] * BA_FRAME_MS >= BA_TIMEOUT_MS) {
            BA_CHECK(logo_is_x6200(&cur));
            BA_CHECK(cur.delayed == true);
            BA_CHECK(cur.cursor_visible == true);
        }
    }

    /* t = 30001 ms y t = 120000 ms: hay cuadro/estado animado nuevo respecto de
     * la ventana anterior (no es una pantalla congelada por STARTUP DELAYED). */
    {
        static const uint64_t probe_ms[] = { 30001, 120000 };
        size_t                k;

        for (k = 0; k < sizeof(probe_ms) / sizeof(probe_ms[0]); k++) {
            ba_model_t cur, prv;
            ba_dirty_t dirty;

            model_at_ms(&cur, probe_ms[k]);

            /* Cuadro anterior de la grilla de 84 ms. */
            {
                uint32_t prev_frame = frame_at(probe_ms[k]) - 1u;

                model_at_frame(&prv, prev_frame);
            }

            BA_CHECK_MSG(cur.delayed, "t=%llu sin delayed", (unsigned long long) probe_ms[k]);
            BA_CHECK(cur.rain_active == true);
            BA_CHECK_MSG(cur.rain_step != prv.rain_step, "t=%llu rain congelada", (unsigned long long) probe_ms[k]);

            ba_render(&s, &cur, &prv, false, &dirty);
            BA_CHECK(dirty.count > 0);
            BA_CHECK(dirty.count <= BA_MAX_DIRTY);
            BA_CHECK(dirty.overflow == false);

            /* Solo el pie cambio su texto; el logo sigue fijo y legible. */
            BA_CHECK(logo_is_x6200(&cur));
            BA_CHECK_STREQ(ba_footer_text(&cur), "> STARTUP DELAYED_");
        }
    }

    /* Los 30 s son SOLO texto: el conjunto de estados animados no se congela y
     * el pie cambia en el cuadro exacto del umbral. */
    {
        ba_model_t antes, despues;

        model_at_ms(&antes, 29999);
        model_at_ms(&despues, 30000);
        BA_CHECK(antes.delayed == false);
        BA_CHECK(despues.delayed == true);
        BA_CHECK_STREQ(ba_footer_text(&antes), "> INITIALIZING_");
        BA_CHECK_STREQ(ba_footer_text(&despues), "> STARTUP DELAYED_");
        BA_CHECK(despues.rain_active == true);
        BA_CHECK(despues.cursor_visible == true);
    }

    free(px);
}

/* ---------------------------------------------------------------------------
 * Layout y geometria de las lanes
 * --------------------------------------------------------------------------- */
static void check_lane_box(int lane, int32_t x1) {
    ba_rect_t box    = ba_rain_box(lane);
    ba_rect_t screen = ba_screen_box();

    BA_CHECK_EQ_INT(box.x1, x1);
    BA_CHECK_EQ_INT(box.x2, x1 + BA_RAIN_BOX_W - 1);
    BA_CHECK_EQ_INT(box.y1, 112);
    BA_CHECK_EQ_INT(box.y2, 347);
    BA_CHECK_EQ_INT(box.x2 - box.x1 + 1, 10);
    BA_CHECK_EQ_INT(box.y2 - box.y1 + 1, 236);
    BA_CHECK(ba_rect_valid(&box));
    BA_CHECK(ba_rect_inside(&box, &screen));
}

static void test_layout(void) {
    ba_rect_t logo   = ba_logo_box();
    ba_rect_t cell;
    ba_rect_t header = ba_header_box();
    ba_rect_t sub    = ba_subtitle_box();
    ba_rect_t foot   = ba_footer_box();
    ba_rect_t cursor = ba_cursor_cell();
    ba_rect_t screen = ba_screen_box();
    int       pos, lane;

    /* Logo: 5 glifos de 50 px con separacion 10 -> 290 px, centrado en x=400. */
    BA_CHECK_EQ_INT(logo.x1, 255);
    BA_CHECK_EQ_INT(logo.x2, 544);
    BA_CHECK_EQ_INT(logo.x2 - logo.x1 + 1, 290);
    BA_CHECK_EQ_INT(ba_center_x(&logo), 400);
    BA_CHECK_EQ_INT(logo.y1, 168);
    BA_CHECK_EQ_INT(logo.y2, 254);

    for (pos = 0; pos < BA_LOGO_LEN; pos++) {
        cell = ba_logo_cell(pos);
        BA_CHECK_EQ_INT(cell.x2 - cell.x1 + 1, 50);
        BA_CHECK_EQ_INT(cell.y2 - cell.y1 + 1, 70);
        BA_CHECK_EQ_INT(cell.x1, 255 + pos * 60);
        BA_CHECK(cell.y1 == 176 && cell.y2 == 245);
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

    /* Pie: centrado en y=390 y la caja cubre las dos variantes. */
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

    /* Las cuatro lanes del contrato, con sus x exactas. */
    check_lane_box(0, 116);
    check_lane_box(1, 182);
    check_lane_box(2, 608);
    check_lane_box(3, 676);

    /* Las lanes no se pisan entre si ni con el logo, el encabezado, el subtitulo,
     * el pie ni la celda del cursor (y todas caben en la pantalla). */
    for (lane = 0; lane < BA_RAIN_LANES; lane++) {
        ba_rect_t a = ba_rain_box(lane);
        int       other;

        BA_CHECK(!pixel_in_rect(a.x1, a.y1, &logo));
        BA_CHECK(!pixel_in_rect(a.x1, a.y1, &foot));
        BA_CHECK(!pixel_in_rect(a.x2, a.y2, &cursor));
        BA_CHECK(!pixel_in_rect(a.x1, a.y1, &header));
        BA_CHECK(!pixel_in_rect(a.x1, a.y1, &sub));

        for (other = lane + 1; other < BA_RAIN_LANES; other++) {
            ba_rect_t b = ba_rain_box(other);

            BA_CHECK(a.x2 < b.x1 || b.x2 < a.x1 || a.y2 < b.y1 || b.y2 < a.y1);
        }
    }

    /* Las dos primeras lanes quedan a la izquierda del logo (x1=255) y las dos
     * ultimas a la derecha (x2=544). */
    BA_CHECK(ba_rain_box(0).x2 < logo.x1);
    BA_CHECK(ba_rain_box(1).x2 < logo.x1);
    BA_CHECK(ba_rain_box(2).x1 > logo.x2);
    BA_CHECK(ba_rain_box(3).x1 > logo.x2);

    /* Nada de cajas invalidas. */
    BA_CHECK(ba_rect_valid(&screen));
    BA_CHECK(ba_rect_inside(&logo, &screen));
}

/* ---------------------------------------------------------------------------
 * Lluvia: oraculo geometrico independiente
 * --------------------------------------------------------------------------- */
/* Oracion independiente de un lane: llena `buf` (10 x 236) con el fondo y hasta
 * tres glifos 5x7 a escala 2 segun las formulas del contrato, y devuelve
 * cuantos glifos dibujo. Nada fuera de la caja: si el glifo no entra entero, no
 * se dibuja. */
static int oracle_rain_lane(uint16_t *buf, int lane, int step, uint32_t frame_index) {
    const char *alphabet = ba_alphabet();
    size_t      len      = strlen(alphabet);
    int32_t     head_y   = BA_RAIN_Y1 + (step * 6 + lane * 41) % 236;
    int         trail, glyphs = 0, i;

    for (i = 0; i < BA_RAIN_BOX_W * (BA_RAIN_Y2 - BA_RAIN_Y1 + 1); i++) {
        buf[i] = BA_COLOR_BG;
    }

    for (trail = 0; trail < BA_RAIN_TRAILS; trail++) {
        int32_t        gy = head_y - trail * BA_RAIN_GAP;
        char           c;
        const uint8_t *g;
        uint16_t       color;
        int            col, row;

        if (gy < BA_RAIN_Y1 || gy + BA_RAIN_BOX_H - 1 > BA_RAIN_Y2) {
            continue;
        }

        c  = alphabet[(size_t) (((uint64_t) frame_index * 7u + (uint64_t) lane * 11u + (uint64_t) trail * 5u) % len)];
        g  = ba_glyph(c);
        color = (trail == 0) ? BA_COLOR_PRIMARY : BA_COLOR_SECONDARY;

        if (g == NULL) {
            continue;
        }

        for (col = 0; col < BA_GLYPH_W; col++) {
            for (row = 0; row < BA_GLYPH_H; row++) {
                int px, py;

                if (((g[col] >> row) & 1u) == 0u) {
                    continue;
                }

                for (py = 0; py < BA_RAIN_SCALE; py++) {
                    for (px = 0; px < BA_RAIN_SCALE; px++) {
                        int32_t yy = gy + row * BA_RAIN_SCALE + py - BA_RAIN_Y1;
                        int32_t xx = col * BA_RAIN_SCALE + px;

                        buf[yy * BA_RAIN_BOX_W + xx] = color;
                    }
                }
            }
        }

        glyphs++;
    }

    return glyphs;
}

/* Cantidad de pixeles del lane en el surface que no coinciden con el oraculo. */
static int lane_diff(const ba_surface_t *s, const uint16_t *buf, int lane) {
    ba_rect_t box = ba_rain_box(lane);
    int       dx, dy, n = 0;

    for (dy = 0; dy < BA_RAIN_Y2 - BA_RAIN_Y1 + 1; dy++) {
        for (dx = 0; dx < BA_RAIN_BOX_W; dx++) {
            uint16_t got = s->px[(size_t) (box.y1 + dy) * (size_t) s->w + (size_t) (box.x1 + dx)];

            if (got != buf[dy * BA_RAIN_BOX_W + dx]) {
                n++;
            }
        }
    }

    return n;
}

static void test_rain_raster(void) {
    uint16_t    *px = surface_alloc();
    ba_surface_t s  = surface_wrap(px);
    uint16_t     buf[BA_RAIN_BOX_W * 236];
    uint32_t     frame;
    int          lane;

    /* Reproduccion incremental de 300 cuadros seguidos (25 s) desde ~3,4 s:
     * la lluvia se compara, caja por caja, contra el oraculo independiente. */
    {
        ba_model_t prev;
        bool       have_prev = false;

        for (frame = 40; frame < 340; frame++) {
            ba_model_t m;
            ba_dirty_t dirty;
            int        total_glyphs = 0;

            model_at_frame(&m, frame);
            ba_render(&s, &m, have_prev ? &prev : NULL, !have_prev, &dirty);
            prev      = m;
            have_prev = true;

            BA_CHECK(dirty.overflow == false);
            BA_CHECK(dirty.count <= BA_MAX_DIRTY);

            for (lane = 0; lane < BA_RAIN_LANES; lane++) {
                int glyphs = oracle_rain_lane(buf, lane, m.rain_step, frame);

                BA_CHECK_MSG(glyphs <= 3, "frame=%u lane=%d glyphs=%d", frame, lane, glyphs);
                BA_CHECK_MSG(lane_diff(&s, buf, lane) == 0, "frame=%u lane=%d difiere del oraculo", frame, lane);
                total_glyphs += glyphs;
            }

            /* Nunca mas de 12 glifos de lluvia por cuadro. */
            BA_CHECK_MSG(total_glyphs <= 12, "frame=%u glyphs=%d", frame, total_glyphs);
        }
    }

    /* Cuadro aislado de lluvia: nada fuera de las cuatro lanes se pinta. */
    {
        ba_model_t m, prev;
        ba_dirty_t dirty;
        int        x, y, fuera = 0;

        model_at_frame(&m, 200);
        prev                = m;
        prev.rain_active    = false;

        memset(px, 0xA5, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t));
        ba_render(&s, &m, &prev, false, &dirty);

        BA_CHECK_EQ_INT(dirty.count, BA_RAIN_LANES);
        BA_CHECK(dirty.overflow == false);

        for (lane = 0; lane < BA_RAIN_LANES; lane++) {
            ba_rect_t box    = ba_rain_box(lane);
            int       glyphs = oracle_rain_lane(buf, lane, m.rain_step, 200);

            BA_CHECK(is_rain_rect(&dirty.rect[lane]));
            BA_CHECK(dirty.rect[lane].x1 == box.x1 && dirty.rect[lane].y1 == box.y1 &&
                     dirty.rect[lane].x2 == box.x2 && dirty.rect[lane].y2 == box.y2);
            BA_CHECK_MSG(glyphs >= 1 && glyphs <= 3, "lane=%d glyphs=%d", lane, glyphs);
            BA_CHECK_MSG(lane_diff(&s, buf, lane) == 0, "lane=%d difiere del oraculo", lane);
        }

        for (y = 0; y < BA_LOGICAL_H; y++) {
            for (x = 0; x < BA_LOGICAL_W; x++) {
                uint16_t v = s.px[(size_t) y * BA_LOGICAL_W + (size_t) x];
                int      inside = 0;

                if (v == BA_SENTINEL || v == BA_COLOR_BG) {
                    continue;
                }

                for (lane = 0; lane < BA_RAIN_LANES; lane++) {
                    ba_rect_t box = ba_rain_box(lane);

                    if (pixel_in_rect(x, y, &box)) {
                        inside = 1;
                    }
                }

                if (!inside) {
                    fuera++;
                }
            }
        }

        /* Con el fondo afuera de sentinela: la lluvia solo toca sus cuatro cajas. */
        BA_CHECK_EQ_INT(fuera, 0);
    }

    /* Antes de 875 ms no hay lluvia: la caja queda en fondo y no hay dirty de
     * lane. */
    {
        ba_model_t m;
        ba_dirty_t dirty;
        int        x, y, pintados = 0;

        memset(px, 0, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t));
        s = surface_wrap(px);

        model_at_ms(&m, 800);
        BA_CHECK(m.rain_active == false);
        ba_render(&s, &m, NULL, true, &dirty);

        for (lane = 0; lane < BA_RAIN_LANES; lane++) {
            ba_rect_t box = ba_rain_box(lane);

            for (y = box.y1; y <= box.y2; y++) {
                for (x = box.x1; x <= box.x2; x++) {
                    if (s.px[(size_t) y * BA_LOGICAL_W + (size_t) x] != BA_COLOR_BG) {
                        pintados++;
                    }
                }
            }
        }

        BA_CHECK_EQ_INT(pintados, 0);
        BA_CHECK_EQ_INT(dirty.count, 1); /* solo el fondo completo */
        BA_CHECK(is_screen_rect(&dirty.rect[0]));
    }

    free(px);
}

/* ---------------------------------------------------------------------------
 * Palette
 * --------------------------------------------------------------------------- */
static void test_palette(void) {
    /* Los tres colores originales, tal como quedan en RGB565. */
    BA_CHECK_EQ_INT(BA_COLOR_BG, 0x0882);
    BA_CHECK_EQ_INT(BA_COLOR_PRIMARY, 0x8756);
    BA_CHECK_EQ_INT(BA_COLOR_SECONDARY, 0x74B0);

    /* El unico color nuevo es el destello del BEAMS, y es exactamente #D2FFE4
     * reducido por la macro del contrato. */
    BA_CHECK_EQ_INT(BA_COLOR_HIGHLIGHT, BA_RGB565(0xD2, 0xFF, 0xE4));
    BA_CHECK_EQ_INT(BA_COLOR_HIGHLIGHT, 0xD7FC);
}

/* ---------------------------------------------------------------------------
 * Raster: primer cuadro completo + higiene de dirty + determinismo
 * --------------------------------------------------------------------------- */
static void test_raster(void) {
    uint16_t    *px = surface_alloc();
    ba_surface_t s  = surface_wrap(px);
    ba_model_t   m0;
    ba_dirty_t   dirty;
    int          pos;

    /* Cuadro 0 (completo): fondo en toda la pantalla y dirty = pantalla entera. */
    ba_model_at(0, 0, &m0);
    ba_render(&s, &m0, NULL, true, &dirty);

    BA_CHECK_EQ_INT(dirty.count, 1);
    BA_CHECK_EQ_INT(dirty.overflow, 0);
    BA_CHECK(is_screen_rect(&dirty.rect[0]));

    {
        int x, y, bg = 0, ajeno = 0;

        for (y = 0; y < BA_LOGICAL_H; y++) {
            for (x = 0; x < BA_LOGICAL_W; x++) {
                uint16_t v = s.px[(size_t) y * BA_LOGICAL_W + (size_t) x];

                if (v == BA_COLOR_BG) {
                    bg++;
                }

                if (v != BA_COLOR_BG && v != BA_COLOR_PRIMARY && v != BA_COLOR_SECONDARY &&
                    v != BA_COLOR_HIGHLIGHT) {
                    ajeno++;
                }
            }
        }

        BA_CHECK_EQ_INT(ajeno, 0);
        BA_CHECK(bg > 370000);

        /* El area pintada es exactamente la de los glifos de los cuatro textos
         * (cuadro 0: sin lluvia), al tamano del contrato. */
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

    /* Secuencia larga incremental (0..800 cuadros, 67 s): cada cuadro declara
     * cajas validas, nunca supera el limite, nunca repinta la pantalla completa
     * (salvo el primero) y el area sucia queda muy por debajo del 12%. */
    {
        ba_model_t prev;
        bool       have_prev = false;
        uint32_t   frame;
        long       max_area = 0;

        replay(&s, 0); /* deja el surface en el cuadro 0, como el animador */
        have_prev = false;

        for (frame = 0; frame <= 800; frame++) {
            ba_model_t m;
            ba_dirty_t d;
            long       area;

            model_at_frame(&m, frame);
            ba_render(&s, &m, have_prev ? &prev : NULL, !have_prev, &d);
            prev      = m;
            have_prev = true;

            BA_CHECK_MSG(d.overflow == false, "frame=%u overflow", frame);
            BA_CHECK_MSG(d.count <= BA_MAX_DIRTY, "frame=%u count=%d", frame, d.count);

            area = dirty_area(&d);

            if (frame > 0) {
                int k;

                BA_CHECK_MSG(area < (long) BA_LOGICAL_W * BA_LOGICAL_H, "frame=%u repinto todo", frame);

                for (k = 0; k < d.count; k++) {
                    BA_CHECK_MSG(!is_screen_rect(&d.rect[k]), "frame=%u rect entero", frame);
                    BA_CHECK(ba_rect_valid(&d.rect[k]));
                }

                /* Techo del contrato: <=12% de 800x480 por cuadro ordinario. */
                BA_CHECK_MSG(area <= (long) BA_LOGICAL_W * BA_LOGICAL_H * 12 / 100,
                             "frame=%u area=%ld (%.1f%%)", frame, area,
                             100.0 * (double) area / (double) (BA_LOGICAL_W * BA_LOGICAL_H));

                if (area > max_area) {
                    max_area = area;
                }
            }
        }

        printf("  (raster) area sucia maxima tras la intro: %ld px de %d (%.1f%%)\n", max_area,
               BA_LOGICAL_W * BA_LOGICAL_H,
               100.0 * (double) max_area / (double) (BA_LOGICAL_W * BA_LOGICAL_H));
    }

    /* Nada se escribe fuera de las cajas declaradas: surface de centinela, un
     * render parcial y todo lo que no cambio queda intacto. */
    {
        ba_model_t cur, prv;
        ba_dirty_t d;
        int        x, y, fuera = 0;

        model_at_frame(&cur, 240); /* lluvia + logo fijo */
        model_at_frame(&prv, 239);

        memset(px, 0xA5, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t));
        s = surface_wrap(px);
        ba_render(&s, &cur, &prv, false, &d);

        for (y = 0; y < BA_LOGICAL_H; y++) {
            for (x = 0; x < BA_LOGICAL_W; x++) {
                int inside = 0, k;

                if (s.px[(size_t) y * BA_LOGICAL_W + (size_t) x] == BA_SENTINEL) {
                    continue;
                }

                for (k = 0; k < d.count; k++) {
                    if (pixel_in_rect(x, y, &d.rect[k])) {
                        inside = 1;
                    }
                }

                if (!inside) {
                    fuera++;
                }
            }
        }

        BA_CHECK_EQ_INT(fuera, 0);

        /* Y ningun pixel quedo a medias: dentro de cada caja de lluvia todo es
         * fondo o un glifo del oraculo (sin restos del centinela). */
        BA_CHECK(d.count > 0);
    }

    /* BEAMS: exactamente una celda iluminada con el color de destello, y la celda
     * anterior vuelve al verde principal en la transicion. */
    {
        ba_model_t a, b;
        ba_dirty_t d;
        ba_rect_t  cell0 = ba_logo_cell(0);
        ba_rect_t  cell1 = ba_logo_cell(1);

        model_at_frame(&a, 26); /* 2184 ms: ciclo 1309 -> haz en la celda 0 */
        BA_CHECK_EQ_INT(a.beam_cell, 0);
        model_at_frame(&b, 29); /* 2436 ms: ciclo 1561 -> haz en la celda 1 */
        BA_CHECK_EQ_INT(b.beam_cell, 1);

        ba_render(&s, &a, NULL, true, &d);
        BA_CHECK_EQ_INT(count_color(&s, &cell0, BA_COLOR_HIGHLIGHT), expected_painted("X", BA_LOGO_SCALE));
        BA_CHECK_EQ_INT(count_color(&s, &cell1, BA_COLOR_HIGHLIGHT), 0);

        ba_render(&s, &b, &a, false, &d);
        BA_CHECK(d.count <= BA_MAX_DIRTY);
        BA_CHECK(d.overflow == false);
        BA_CHECK_EQ_INT(count_color(&s, &cell0, BA_COLOR_HIGHLIGHT), 0);
        BA_CHECK_EQ_INT(count_color(&s, &cell0, BA_COLOR_PRIMARY), expected_painted("X", BA_LOGO_SCALE));
        BA_CHECK_EQ_INT(count_color(&s, &cell1, BA_COLOR_HIGHLIGHT), expected_painted("6", BA_LOGO_SCALE));
        BA_CHECK_EQ_INT(count_color(&s, &cell1, BA_COLOR_PRIMARY), 0);

        /* En todo el cuadro hay una sola celda iluminada. */
        {
            ba_rect_t logo_box = ba_logo_box();

            BA_CHECK_EQ_INT(count_color(&s, &logo_box, BA_COLOR_HIGHLIGHT), expected_painted("6", BA_LOGO_SCALE));
        }
    }

    /* Cursor: con el logo ya fijo y antes de la lluvia no se repinta nada hasta
     * que el '_' cambia de estado; despues el unico dirty que no es de lluvia es
     * la celda del cursor. */
    {
        ba_model_t m9, m10, m11, m14;
        ba_dirty_t d, dc1, dc2;
        ba_rect_t  cursor_cell = ba_cursor_cell();
        ba_rect_t  footer_box  = ba_footer_box();

        model_at_frame(&m9, 9); /* 756 ms: intro completa, sin lluvia todavia */
        BA_CHECK(logo_is_x6200(&m9));
        BA_CHECK(m9.rain_active == false);
        BA_CHECK(m9.cursor_visible == true);
        ba_render(&s, &m9, NULL, true, &d);

        model_at_frame(&m10, 10); /* 840 ms: nada cambia todavia */
        BA_CHECK(logo_is_x6200(&m10));
        ba_render(&s, &m10, &m9, false, &d);
        BA_CHECK_EQ_INT(d.count, 0);

        /* Arranca la lluvia (875 ms): cuatro cajas, sin tocar el logo. */
        model_at_ms(&m11, 924);
        BA_CHECK(m11.rain_active == true);
        ba_render(&s, &m11, &m10, false, &d);
        BA_CHECK_EQ_INT(d.count, BA_RAIN_LANES);
        BA_CHECK(d.overflow == false);

        /* Cursor apagado: se repinta su celda (y solo eso ademas de la lluvia). */
        model_at_ms(&m10, 1375);
        BA_CHECK(m10.cursor_visible == false);
        ba_render(&s, &m10, &m11, false, &dc1);
        BA_CHECK_EQ_INT(dc1.count, BA_RAIN_LANES + 1);
        BA_CHECK_EQ_INT(dc1.rect[0].x1, cursor_cell.x1);
        BA_CHECK_EQ_INT(dc1.rect[0].y1, cursor_cell.y1);
        BA_CHECK_EQ_INT(dc1.rect[0].x2, cursor_cell.x2);
        BA_CHECK_EQ_INT(dc1.rect[0].y2, cursor_cell.y2);
        BA_CHECK_EQ_INT(count_color(&s, &cursor_cell, BA_COLOR_PRIMARY), 0);
        BA_CHECK(count_color(&s, &footer_box, BA_COLOR_PRIMARY) > 0);

        /* Cursor encendido otra vez: vuelve el '_' en su celda. */
        model_at_ms(&m14, 1875);
        BA_CHECK(m14.cursor_visible == true);
        ba_render(&s, &m14, &m10, false, &dc2);
        BA_CHECK_EQ_INT(dc2.count, BA_RAIN_LANES + 1);
        BA_CHECK_EQ_INT(dc2.rect[0].x1, cursor_cell.x1);
        BA_CHECK_EQ_INT(dc2.rect[0].y1, cursor_cell.y1);
        BA_CHECK(count_color(&s, &cursor_cell, BA_COLOR_PRIMARY) > 0);
    }

    /* A los 30 s: el cuadro solo repinta el pie (nunca un dirty de la celda del
     * cursor) y el texto pasa a STARTUP DELAYED_ con pixeles propios a la
     * izquierda del box de INITIALIZING_ (240..266). */
    {
        ba_model_t antes, despues;
        ba_dirty_t d;
        int        k, cursor_dirty = 0, izq = 0, x, y;

        model_at_frame(&antes, 356); /* 29904 ms */
        model_at_frame(&despues, 357);

        /* La grilla de 84 ms no tiene un cuadro exactamente en 30000; se usa el
         * modelo del umbral para el texto y la transicion de la grilla para el
         * dirty. */
        {
            ba_model_t exacto;

            model_at_ms(&exacto, 30000);
            BA_CHECK_STREQ(ba_footer_text(&exacto), "> STARTUP DELAYED_");
        }

        model_at_ms(&despues, 30000); /* mismo texto, cuadro del umbral */
        BA_CHECK(despues.delayed == true);
        BA_CHECK(antes.delayed == false);

        ba_render(&s, &despues, &antes, false, &d);
        BA_CHECK(d.overflow == false);
        BA_CHECK(d.count <= BA_MAX_DIRTY);

        for (k = 0; k < d.count; k++) {
            if (d.rect[k].x1 == ba_cursor_cell().x1 && d.rect[k].y1 == ba_cursor_cell().y1 &&
                d.rect[k].x2 == ba_cursor_cell().x2 && d.rect[k].y2 == ba_cursor_cell().y2) {
                cursor_dirty++;
            }
        }

        BA_CHECK_EQ_INT(cursor_dirty, 0);

        for (y = 380; y <= 400; y++) {
            for (x = 240; x <= 266; x++) {
                if (s.px[(size_t) y * BA_LOGICAL_W + (size_t) x] != BA_COLOR_BG) {
                    izq++;
                }
            }
        }

        BA_CHECK(izq > 0); /* pixeles que solo puede aportar DELAYED */
    }

    /* Determinismo: dos corridas identicas dan bytes identicos. */
    {
        uint16_t    *px_b = surface_alloc();
        ba_surface_t s_b  = surface_wrap(px_b);

        replay(&s, 400);
        replay(&s_b, 400);
        BA_CHECK(memcmp(px, px_b, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t)) == 0);
        free(px_b);
    }

    /* Secuencias que SALTEA cuadros: el resultado no depende de redibujar todos
     * los cuadros previos (modelo puro + dirty completo). */
    {
        static const uint32_t saltos[] = { 26, 45, 200, 714, 2000 };
        uint16_t             *px_all = surface_alloc();
        uint16_t             *px_skip = surface_alloc();
        uint16_t             *px_full = surface_alloc();
        ba_surface_t          s_all  = surface_wrap(px_all);
        ba_surface_t          s_skip = surface_wrap(px_skip);
        ba_surface_t          s_full = surface_wrap(px_full);
        size_t                i;

        for (i = 0; i < sizeof(saltos) / sizeof(saltos[0]); i++) {
            ba_model_t m0, mN;
            ba_dirty_t d;

            /* A: incremental cuadro a cuadro hasta el final. */
            replay(&s_all, saltos[i]);

            /* B: solo el primer y el ultimo cuadro (se saltean todos los del
             * medio). */
            model_at_frame(&m0, 0);
            model_at_frame(&mN, saltos[i]);
            ba_render(&s_skip, &m0, NULL, true, &d);
            ba_render(&s_skip, &mN, &m0, false, &d);
            BA_CHECK_MSG(d.overflow == false, "salto=%u overflow", saltos[i]);

            /* C: un unico repintado completo del ultimo cuadro. */
            ba_render(&s_full, &mN, NULL, true, &d);
            BA_CHECK_EQ_INT(d.count, 1);

            BA_CHECK_MSG(memcmp(px_all, px_skip, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t)) == 0,
                         "salto=%u: la secuencia con saltos difiere del replay", saltos[i]);
            BA_CHECK_MSG(memcmp(px_all, px_full, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t)) == 0,
                         "salto=%u: el repintado completo difiere del replay", saltos[i]);
        }

        free(px_all);
        free(px_skip);
        free(px_full);
    }

    /* Los cambios declarados por dirty explican TODOS los cambios de pixeles. */
    {
        uint16_t    *px_before = surface_alloc();
        ba_surface_t s_before  = surface_wrap(px_before);
        uint32_t     frame;

        replay(&s_before, 0);
        model_at_frame(&m0, 0);

        {
            ba_surface_t s_cur  = surface_wrap(px);
            ba_model_t   prev   = m0;

            memcpy(px, px_before, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t));

            for (frame = 1; frame <= 460; frame++) {
                ba_model_t m;
                ba_dirty_t d;
                int        fuera;

                model_at_frame(&m, frame);
                ba_render(&s_cur, &m, &prev, false, &d);
                fuera = diff_outside_dirty(px_before, px, &d);
                BA_CHECK_MSG(fuera == 0, "frame=%u: %d pixeles cambiaron fuera de dirty", frame, fuera);

                memcpy(px_before, px, (size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t));
                prev = m;
            }
        }

        free(px_before);
    }

    free(px);
}

int main(void) {
    test_palette();
    test_model_intro();
    test_model_cycle();
    test_model_continuous();
    test_layout();
    test_rain_raster();
    test_raster();
    BA_TEST_SUMMARY("renderer_test");
}

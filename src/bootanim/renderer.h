/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Animacion de arranque neo-retro del X6200 (issue #192).
 *
 *  Modelo de animacion + raster PURO a un buffer logico 800x480 RGB565. No usa
 *  LVGL, hardware, heap, red ni relojes: todas las funciones escriben solo en
 *  los buffers que reciben, asi que el test de host (tests/bootanim/) las
 *  ejercita en memoria y compara byte a byte.
 *
 *  Los numeros del contrato (escalas, marcas de bloqueo, cajas, textos) viven
 *  aca UNA vez; los tests los verifican contra la especificacion del issue.
 */
#ifndef X6200_BOOTANIM_RENDERER_H
#define X6200_BOOTANIM_RENDERER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Geometria logica (la misma que la GUI: 800x480 landscape) --------------- */
#define BA_LOGICAL_W 800
#define BA_LOGICAL_H 480

/* --- Reloj / tiempos del contrato (secciones 3 y 4 del issue) ---------------- */
#define BA_FRAME_MS     84u    /* 84 ms por cuadro: 11,9 fps, nunca mas de 12 */
#define BA_TIMEOUT_MS   30000u /* a los 30 s el pie pasa a STARTUP DELAYED_: SOLO texto */
#define BA_LOGO_LEN     5      /* las cinco posiciones de caracteres del logo */
#define BA_CURSOR_MS    500u   /* cursor visible 500 ms, invisible 500 ms */
#define BA_LOGO_FULL_MS 875u   /* desde aca: logo fijo, lluvia activa y ciclos de 6 s */

/* Ciclo visual que se repite para siempre despues de la intro (base temporal
 * BA_LOGO_FULL_MS): 0..1199 logo fijo + lluvia; 1200..2599 BEAMS; 2600..3599
 * DECRYPT; 3600..5999 logo fijo + lluvia. La intro 0..874 ms NO se reejecuta. */
#define BA_CYCLE_MS        6000u
#define BA_BEAMS_MS        1200u /* primer ciclo en el que avanza el haz */
#define BA_BEAMS_END_MS    2600u
#define BA_BEAM_STEP_MS    280u  /* el haz avanza una celda cada 280 ms */
#define BA_DECRYPT_MS      2600u /* primera posicion provisional del DECRYPT */
#define BA_DECRYPT_END_MS  3600u
#define BA_DECRYPT_STEP_MS 200u  /* el DECRYPT pasa de celda cada 200 ms */

/* Lluvia continua (RAIN): cuatro columnas fijas, glifos 5x7 a escala 2 (10x14),
 * separados 22 px, hasta 3 por lane (lider + dos estelas) y nunca mas de 12
 * glifos por cuadro. Las cajas son INCLUSIVAS y el glifo se dibuja solo si su
 * caja entera [y, y+13] cabe adentro. */
#define BA_RAIN_LANES  4
#define BA_RAIN_STEPS  40 /* pasos verticales antes de repetir (0..39) */
#define BA_RAIN_SCALE  2
#define BA_RAIN_TRAILS 3
#define BA_RAIN_GAP    22
#define BA_RAIN_Y1     112
#define BA_RAIN_Y2     347
#define BA_RAIN_BOX_W  10 /* 5 columnas * BA_RAIN_SCALE */
#define BA_RAIN_BOX_H  14 /* 7 filas * BA_RAIN_SCALE */

/* --- Paleta (contrato: negro casi absoluto, fosforo y verde apagado) --------- */
#define BA_RGB565(r8, g8, b8)                                                         \
    ((uint16_t) ((((uint32_t) (r8) & 0xF8u) << 8) | (((uint32_t) (g8) & 0xFCu) << 3) | \
                 ((uint32_t) (b8) >> 3)))

#define BA_COLOR_BG        BA_RGB565(0x0B, 0x10, 0x12) /* #0B1012 */
#define BA_COLOR_PRIMARY   BA_RGB565(0x85, 0xE9, 0xB2) /* #85E9B2 */
#define BA_COLOR_SECONDARY BA_RGB565(0x72, 0x97, 0x84) /* #729784 */
#define BA_COLOR_HIGHLIGHT BA_RGB565(0xD2, 0xFF, 0xE4) /* #D2FFE4: celda iluminada por BEAMS */

/* --- Layout en coordenadas logicas (contrato seccion 0) ----------------------
 * Escala entera sobre un bitmap mono 5x7; avance de glifo 5*escala y separacion
 * 1*escala (o sea 6*escala por caracter). El encabezado se ancla por baseline,
 * el subtitulo y el pie por el centro vertical de su caja, y el logo dentro de
 * su caja (168..254): la lectura del contrato queda codificada aca y verificada
 * por los tests. */
#define BA_GLYPH_W         5
#define BA_GLYPH_H         7
#define BA_GLYPH_ADV       6 /* glifo + separacion, en unidades de escala */
#define BA_LOGO_SCALE      10
#define BA_TEXT_SCALE      3
#define BA_LOGO_BOX_Y1     168
#define BA_LOGO_BOX_Y2     254
#define BA_HEADER_BASELINE 70
#define BA_SUBTITLE_CY     297
#define BA_FOOTER_CY       390

/* --- Tipos ------------------------------------------------------------------ */
typedef struct {
    int32_t x1, y1, x2, y2; /* limites INCLUSIVOS, coordenadas logicas */
} ba_rect_t;

typedef struct {
    uint16_t *px;    /* BA_LOGICAL_W * BA_LOGICAL_H valores RGB565, fila por fila */
    int32_t   w, h;
} ba_surface_t;

typedef struct {
    char     logo[BA_LOGO_LEN];        /* caracter visible en cada posicion */
    bool     logo_locked[BA_LOGO_LEN]; /* true = fijado (el DECRYPT lo libera 1 cuadro) */
    bool     cursor_visible;           /* '_' del pie visible */
    bool     delayed;                  /* >30 s: SOLO cambia el texto del pie, no la salida */
    int      beam_cell;                /* BEAMS: 0..4 celda iluminada; -1 = ninguna */
    bool     rain_active;              /* lluvia activa (desde BA_LOGO_FULL_MS) */
    int      rain_step;                /* 0..BA_RAIN_STEPS-1: avance vertical de la lluvia */
    uint32_t frame_index;              /* el mismo frame con el que se calculo el modelo */
} ba_model_t;

/* Hasta 12 cajas: cuatro lanes de lluvia + las cinco celdas del logo + pie/cursor.
 * `overflow` marca que se intento empujar una caja de mas: el contrato no pierde
 * rectangulos en silencio, el banco lo trata como error (dirty.overflow). */
#define BA_MAX_DIRTY 12
typedef struct {
    ba_rect_t rect[BA_MAX_DIRTY];
    int       count;
    bool      overflow;
} ba_dirty_t;

/* --- Modelo (puro: frame + tiempo -> caracteres y cursor) -------------------- */
void        ba_model_at(uint32_t frame_index, uint64_t elapsed_ms, ba_model_t *out);
const char *ba_alphabet(void);
const char *ba_logo_text(void);
const char *ba_header_text(void);
const char *ba_subtitle_text(void);
const char *ba_footer_text(const ba_model_t *m);
uint16_t    ba_logo_lock_ms(int pos);
char        ba_logo_final_char(int pos);

/* --- Layout (calculado desde el ancho del texto, sin x magicas) -------------- */
int32_t   ba_text_width(const char *text, int32_t scale);
ba_rect_t ba_text_box(const char *text, int32_t scale, int32_t center_x, int32_t top_y);
ba_rect_t ba_logo_box(void);
ba_rect_t ba_logo_cell(int pos);
ba_rect_t ba_header_box(void);
ba_rect_t ba_subtitle_box(void);
ba_rect_t ba_footer_box(void);
ba_rect_t ba_cursor_cell(void);
ba_rect_t ba_rain_box(int lane);
ba_rect_t ba_screen_box(void);
int32_t   ba_center_x(const ba_rect_t *r);

/* --- Fuente bitmap 5x7 (sin archivo externo, sin licencia adicional) --------- */
const uint8_t *ba_glyph(char c);
bool           ba_text_supported(const char *text);

/* --- Raster ----------------------------------------------------------------- */
bool ba_rect_valid(const ba_rect_t *r);
bool ba_rect_inside(const ba_rect_t *inner, const ba_rect_t *outer);

/* Rasteriza `m` sobre `s`. Con full=true (o prev=NULL) pinta el fondo de toda
 * la pantalla, los textos y (si la lluvia esta activa) los cuatro lanes: es el
 * primer cuadro, un unico rectangulo sucio. Si no, repinta solo las cajas que
 * cambiaron respecto de `prev` (celdas del logo con el color del BEAMS, el pie o
 * el cursor y las cuatro cajas fijas de lluvia), borrando al fondo antes de
 * reimprimir. Deja en `dirty` los rectangulos logicos tocados, para que el
 * backend copie solo eso al framebuffer fisico. */
void ba_render(ba_surface_t *s, const ba_model_t *m, const ba_model_t *prev, bool full,
               ba_dirty_t *dirty);

#ifdef __cplusplus
}
#endif

#endif /* X6200_BOOTANIM_RENDERER_H */

/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Tests de host del backend de framebuffer (issue #192, seccion 7): validacion
 *  de geometry/bpp/bitfields/stride/smem, rotacion 90 grados contra un oraculo
 *  independiente, formatos 16 y 32 bpp con bitfields distintos, line_length con
 *  padding, offsets y bytes centinela. No abre /dev/fb0: las estructuras de
 *  ioctl son sinteticas y el framebuffer es memoria.
 */
#define _POSIX_C_SOURCE 200809L /* mkstemp/chmod/geteuid con -std=c11 estricto */

#include "fb.h"
#include "test_util.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define SENTINEL 64

static uint16_t img[BA_LOGICAL_W * BA_LOGICAL_H]; /* surface de trabajo (BSS) */

typedef struct {
    uint8_t  *raw;
    size_t    len;
    uint32_t  line;
    ba_fb_t   fb;
} fake_fb_t;

static void fake_fb_geom(fake_fb_t *f, uint32_t bpp, uint32_t pad, uint32_t xoff, uint32_t yoff,
                         int bitfield_variant) {
    f->line = (uint32_t) BA_PHYS_W * (bpp / 8u) + pad;
    f->fb.geom.xres            = BA_PHYS_W;
    f->fb.geom.yres            = BA_PHYS_H;
    f->fb.geom.bits_per_pixel  = bpp;
    f->fb.geom.bytes_per_pixel = (uint8_t) (bpp / 8u);
    f->fb.geom.line_length     = f->line;
    f->fb.geom.smem_len        = (uint32_t) f->line * BA_PHYS_H;
    f->fb.geom.xoffset         = xoff;
    f->fb.geom.yoffset         = yoff;

    if (bpp == 16u) {
        f->fb.geom.red.offset   = 11;
        f->fb.geom.red.length   = 5;
        f->fb.geom.green.offset = 5;
        f->fb.geom.green.length = 6;
        f->fb.geom.blue.offset  = 0;
        f->fb.geom.blue.length  = 5;
    } else if (bitfield_variant == 1) {
        /* XRGB8888 como lo ve la memoria little endian: B en 0, G en 8, R en 16 */
        f->fb.geom.blue.offset  = 0;
        f->fb.geom.blue.length  = 8;
        f->fb.geom.green.offset = 8;
        f->fb.geom.green.length = 8;
        f->fb.geom.red.offset   = 16;
        f->fb.geom.red.length   = 8;
    } else {
        /* Otro layout valido (R en 0, G en 8, B en 16, alfa en 24) */
        f->fb.geom.red.offset    = 0;
        f->fb.geom.red.length    = 8;
        f->fb.geom.green.offset  = 8;
        f->fb.geom.green.length  = 8;
        f->fb.geom.blue.offset   = 16;
        f->fb.geom.blue.length   = 8;
        f->fb.geom.transp.offset = 24;
        f->fb.geom.transp.length = 8;
        f->fb.geom.has_transp    = true;
    }
}

static void fake_fb_init(fake_fb_t *f, uint32_t bpp, uint32_t pad, uint32_t xoff, uint32_t yoff,
                         int bitfield_variant) {
    size_t i;

    memset(f, 0, sizeof(*f));
    fake_fb_geom(f, bpp, pad, xoff, yoff, bitfield_variant);

    f->len = SENTINEL + (size_t) f->line * BA_PHYS_H + SENTINEL;
    f->raw = malloc(f->len);

    if (f->raw == NULL) {
        printf("FAIL: sin memoria para el framebuffer falso\n");
        exit(1);
    }

    for (i = 0; i < f->len; i++) {
        f->raw[i] = 0x5A; /* centinela en TODO el buffer, adentro y afuera del mapeo */
    }

    f->fb.mem      = f->raw + SENTINEL;
    f->fb.map_len  = (size_t) f->line * BA_PHYS_H;
    f->fb.attached = true;
}

static void fake_fb_free(fake_fb_t *f) {
    free(f->raw);
    f->raw = NULL;
}

static uint32_t fake_read_px(const fake_fb_t *f, uint32_t x, uint32_t y) {
    uint32_t       v = 0;
    const uint8_t *p = f->raw + SENTINEL + (size_t) y * f->line + (size_t) x * f->fb.geom.bytes_per_pixel;

    memcpy(&v, p, f->fb.geom.bytes_per_pixel);

    return v;
}

/* Oracion independiente del empaquetado: escribe en `out` (mismo tamano que el
 * mapeo) el valor esperado de cada pixel logico del rectangulo, respetando
 * stride y offsets. Despues se compara con memcmp contra lo que escribio el
 * backend: eso prueba posiciones, stride, padding y limites de una vez. */
static void expected_blit(uint8_t *out, size_t out_len, const ba_fb_geom_t *geo, const ba_surface_t *src,
                          const ba_rect_t *rect) {
    int32_t x, y;

    memset(out, 0x5A, out_len);

    for (y = rect->y1; y <= rect->y2; y++) {
        for (x = rect->x1; x <= rect->x2; x++) {
            int32_t  fx = y;
            int32_t  fy = BA_LOGICAL_W - 1 - x;
            uint16_t in = src->px[(size_t) y * (size_t) src->w + (size_t) x];
            uint32_t r5 = ((uint32_t) in >> 11) & 0x1Fu;
            uint32_t g6 = ((uint32_t) in >> 5) & 0x3Fu;
            uint32_t b5 = (uint32_t) in & 0x1Fu;
            uint32_t r8 = (r5 * 255u + 15u) / 31u;
            uint32_t g8 = (g6 * 255u + 31u) / 63u;
            uint32_t b8 = (b5 * 255u + 15u) / 31u;
            uint32_t out_px = 0;
            size_t   off;

            out_px |= ((r8 * ((1u << geo->red.length) - 1u) + 127u) / 255u) << geo->red.offset;
            out_px |= ((g8 * ((1u << geo->green.length) - 1u) + 127u) / 255u) << geo->green.offset;
            out_px |= ((b8 * ((1u << geo->blue.length) - 1u) + 127u) / 255u) << geo->blue.offset;

            off = (size_t) (fy + (int32_t) geo->yoffset) * geo->line_length +
                  (size_t) (fx + (int32_t) geo->xoffset) * geo->bytes_per_pixel;

            memcpy(out + off, &out_px, geo->bytes_per_pixel);
        }
    }
}

static void test_geom_ok(void) {
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    ba_fb_geom_t             g;

    memset(&fix, 0, sizeof(fix));
    memset(&var, 0, sizeof(var));

    /* 16 bpp RGB565 con padding de stride. */
    fix.line_length     = (uint32_t) BA_PHYS_W * 2u + 64u;
    fix.smem_len        = fix.line_length * BA_PHYS_H;
    var.xres            = BA_PHYS_W;
    var.yres            = BA_PHYS_H;
    var.bits_per_pixel  = 16;
    var.red.offset      = 11;
    var.red.length      = 5;
    var.green.offset    = 5;
    var.green.length    = 6;
    var.blue.offset     = 0;
    var.blue.length     = 5;

    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_OK);
    BA_CHECK_EQ_INT(g.bytes_per_pixel, 2);
    BA_CHECK_EQ_INT(g.line_length, fix.line_length);
    BA_CHECK_EQ_INT(g.red.offset, 11);
    BA_CHECK_EQ_INT(g.green.length, 6);

    /* 32 bpp XRGB8888. */
    fix.line_length     = (uint32_t) BA_PHYS_W * 4u;
    fix.smem_len        = fix.line_length * BA_PHYS_H;
    var.bits_per_pixel  = 32;
    var.red.offset      = 16;
    var.red.length      = 8;
    var.green.offset    = 8;
    var.green.length    = 8;
    var.blue.offset     = 0;
    var.blue.length     = 8;

    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_OK);
    BA_CHECK_EQ_INT(g.bytes_per_pixel, 4);
    BA_CHECK(g.has_transp == false);

    /* 32 bpp con alfa declarado: se acepta y no pisa el RGB. */
    var.transp.offset = 24;
    var.transp.length = 8;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_OK);
    BA_CHECK(g.has_transp == true);
    BA_CHECK_EQ_INT(g.transp.offset, 24);

    /* 32 bpp con alfa encimado al rojo: se rechaza. */
    var.transp.offset = 16;
    var.transp.length = 8;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_BITFIELDS);
}

static void test_geom_negatives(void) {
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    ba_fb_geom_t             g;

#define RESET_GEOM()                                                                   \
    do {                                                                               \
        memset(&fix, 0, sizeof(fix));                                                  \
        memset(&var, 0, sizeof(var));                                                  \
        fix.line_length    = (uint32_t) BA_PHYS_W * 2u;                                \
        fix.smem_len       = fix.line_length * BA_PHYS_H;                              \
        var.xres           = BA_PHYS_W;                                                \
        var.yres           = BA_PHYS_H;                                                \
        var.bits_per_pixel = 16;                                                       \
        var.red.offset     = 11;                                                       \
        var.red.length     = 5;                                                        \
        var.green.offset   = 5;                                                        \
        var.green.length   = 6;                                                        \
        var.blue.offset    = 0;                                                        \
        var.blue.length    = 5;                                                        \
    } while (0)

    RESET_GEOM();
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_OK); /* control */

    RESET_GEOM();
    var.xres = 640;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_RESOLUTION);

    RESET_GEOM();
    var.yres = 480;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_RESOLUTION);

    RESET_GEOM();
    var.bits_per_pixel = 8;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_BPP);

    RESET_GEOM();
    var.bits_per_pixel = 24;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_BPP);

    RESET_GEOM();
    var.xoffset = 16;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_OFFSET);

    RESET_GEOM();
    var.yoffset = 8;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_OFFSET);

    RESET_GEOM();
    fix.line_length = (uint32_t) BA_PHYS_W * 2u - 2u;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_STRIDE);

    RESET_GEOM();
    fix.smem_len = fix.line_length * BA_PHYS_H - 1u;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_MEMORY);

    RESET_GEOM();
    fix.smem_len = 0u;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_MEMORY);

    /* Overflow potencial: stride gigante por 800 filas se rechaza sin desbordar. */
    RESET_GEOM();
    fix.line_length = 0xFFFFFF00u;
    fix.smem_len    = 0xFFFFFFFFu;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_STRIDE);

    RESET_GEOM();
    fix.line_length = 4u * 1024u * 1024u + 2u;
    fix.smem_len    = 0xFFFFFFFFu;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_STRIDE);

    /* smem_len gigante pero real (mas de 64 MiB) tambien se rechaza. */
    RESET_GEOM();
    fix.smem_len = 0xFFFFFFFFu;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_OK); /* 2 bytes/px: 800 KiB */

    RESET_GEOM();
    var.green.length = 0;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_BITFIELDS);

    RESET_GEOM();
    var.green.offset = 11;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_BITFIELDS);

    RESET_GEOM();
    var.red.offset = 8;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_BITFIELDS);

    RESET_GEOM();
    var.red.offset = 13;
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_BITFIELDS);

    RESET_GEOM();
    var.green.length = 5; /* un 16bpp que no es 5/6/5 */
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_BITFIELDS);

    RESET_GEOM();
    var.blue.offset = 6; /* azul encimado al verde */
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, &g), BA_FB_ERR_BITFIELDS);

    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, &var, NULL), BA_FB_ERR_IOCTL);
    BA_CHECK_EQ_INT(ba_fb_geom_from(NULL, &var, &g), BA_FB_ERR_IOCTL);
    BA_CHECK_EQ_INT(ba_fb_geom_from(&fix, NULL, &g), BA_FB_ERR_IOCTL);

#undef RESET_GEOM
}

/* Oracion independiente del mapeo: transcripcion del algoritmo de rotacion por
 * software de LVGL 8.3.11 (lvgl/src/core/lv_refr.c: draw_buf_rotate_90 y el
 * ajuste del area para LV_DISP_ROT_90 en el tramo 1204-1211), con el
 * particionado en trozos que LVGL hace usando LV_DISP_ROT_MAX_BUF (lv_conf.h
 * del proyecto) y lv_color_t de 32 bits. Deja la imagen fisica 480x800 que
 * tiene que quedar en el framebuffer de la radio. */
static void lvgl_rot90_oracle(const uint16_t *src, uint16_t *dst) {
    const int       area_w  = BA_LOGICAL_W;
    const int       area_h  = BA_LOGICAL_H;
    const int       max_row = (4 * 800 * 50 / 4) / area_w; /* LV_DISP_ROT_MAX_BUF / sizeof(lv_color_t) */
    static uint16_t rot[BA_LOGICAL_W * 64];
    const uint16_t *sp = src;
    int             row = 0;

    while (row < area_h) {
        int h_c = (area_h - row < max_row) ? area_h - row : max_row;
        int y, x;

        /* draw_buf_rotate_90(invert_i = false, area_w, h_c, src, rot) */
        for (y = 0; y < h_c; y++) {
            int i = (area_w - 1) * h_c + y;

            for (x = 0; x < area_w; x++) {
                rot[i] = sp[y * area_w + x];
                i -= h_c;
            }
        }

        /* El trozo se flushea como rectangulo fisico: x en [row, row+h_c-1], y en [0, area_w-1]. */
        for (y = 0; y < area_w; y++) {
            for (x = 0; x < h_c; x++) {
                dst[(size_t) y * BA_PHYS_W + (size_t) (row + x)] = rot[y * h_c + x];
            }
        }

        sp += area_w * h_c;
        row += h_c;
    }
}

static void test_rotation_oracle(void) {
    uint16_t    *esperado = malloc((size_t) BA_PHYS_W * BA_PHYS_H * sizeof(uint16_t));
    fake_fb_t    fb;
    ba_surface_t surface;
    ba_rect_t    screen = { 0, 0, BA_LOGICAL_W - 1, BA_LOGICAL_H - 1 };
    int          i;

    if (esperado == NULL) {
        printf("FAIL: sin memoria\n");
        exit(1);
    }

    /* Imagen logica determinista (sin PRNG). */
    for (i = 0; i < BA_LOGICAL_W * BA_LOGICAL_H; i++) {
        img[i] = (uint16_t) ((i * 31 + (i / BA_LOGICAL_W) * 7) & 0xFFFFu);
    }

    lvgl_rot90_oracle(img, esperado);

    fake_fb_init(&fb, 16, 0, 0, 0, 0);
    surface.px = img;
    surface.w  = BA_LOGICAL_W;
    surface.h  = BA_LOGICAL_H;

    BA_CHECK(ba_fb_blit(&fb.fb, &surface, &screen));
    BA_CHECK_MSG(memcmp(fb.fb.mem, esperado, (size_t) BA_PHYS_W * BA_PHYS_H * 2u) == 0,
                 "la rotacion no coincide con el oraculo de LVGL");

    /* Centinelas del buffer fisico intactos (nada se escribe fuera del mapeo). */
    {
        int fuera = 0;

        for (i = 0; i < SENTINEL; i++) {
            if (fb.raw[i] != 0x5A || fb.raw[SENTINEL + fb.fb.map_len + (size_t) i] != 0x5A) {
                fuera++;
            }
        }

        BA_CHECK_EQ_INT(fuera, 0);
    }

    fake_fb_free(&fb);

    /* Los cuatro extremos y la polaridad. */
    {
        int fx = -1, fy = -1;

        ba_fb_map_point(0, 0, &fx, &fy);
        BA_CHECK_EQ_INT(fx, 0);
        BA_CHECK_EQ_INT(fy, 799);

        ba_fb_map_point(799, 0, &fx, &fy);
        BA_CHECK_EQ_INT(fx, 0);
        BA_CHECK_EQ_INT(fy, 0);

        ba_fb_map_point(0, 479, &fx, &fy);
        BA_CHECK_EQ_INT(fx, 479);
        BA_CHECK_EQ_INT(fy, 799);

        ba_fb_map_point(799, 479, &fx, &fy);
        BA_CHECK_EQ_INT(fx, 479);
        BA_CHECK_EQ_INT(fy, 0);
    }

    /* La palabra X6200: la fila logica cae en una COLUMNA fisica y el texto
     * queda derecho (la fila fisica decrece al avanzar el texto, sin espejo ni
     * volteo). */
    {
        const char *texto   = "X6200";
        int         k, previo_fy = -1, decreciente = 1;
        fake_fb_t   fb2;

        for (i = 0; i < BA_LOGICAL_W * BA_LOGICAL_H; i++) {
            img[i] = BA_COLOR_BG;
        }

        for (k = 0; texto[k] != '\0'; k++) {
            img[(size_t) 300 * BA_LOGICAL_W + (size_t) (100 + k * 2)] = BA_COLOR_PRIMARY;
        }

        fake_fb_init(&fb2, 16, 0, 0, 0, 0);
        surface.px = img;

        BA_CHECK(ba_fb_blit(&fb2.fb, &surface, &screen));

        for (k = 0; k < 5; k++) {
            uint32_t x  = 100u + (uint32_t) k * 2u;
            uint32_t fy = (uint32_t) (BA_LOGICAL_W - 1) - x;

            BA_CHECK_EQ_INT(fake_read_px(&fb2, 300, fy) & 0xFFFFu, BA_COLOR_PRIMARY);
            BA_CHECK_EQ_INT(fake_read_px(&fb2, 300, (uint32_t) (BA_LOGICAL_W - 1) - x - 1u) & 0xFFFFu,
                            BA_COLOR_BG);

            if (previo_fy >= 0 && (int) fy >= previo_fy) {
                decreciente = 0;
            }

            previo_fy = (int) fy;
        }

        BA_CHECK_MSG(decreciente == 1, "el texto no queda derecho (orden fisico invertido)");

        fake_fb_free(&fb2);
    }

    free(esperado);
}

static void test_blit_padding_and_formats(void) {
    ba_surface_t surface;
    int          i;

    for (i = 0; i < BA_LOGICAL_W * BA_LOGICAL_H; i++) {
        img[i] = (uint16_t) (i & 0x7FFFu);
    }

    surface.px = img;
    surface.w  = BA_LOGICAL_W;
    surface.h  = BA_LOGICAL_H;

    /* 16bpp con padding de 64 bytes por linea: la copia tiene que ser
     * byte a byte la esperada (stride con padding respetado, centinelas
     * intactos, nada fuera del rectangulo). */
    {
        fake_fb_t fb;
        ba_rect_t rect = { 10, 20, 29, 39 };
        uint8_t  *esperado;
        size_t    len;

        fake_fb_init(&fb, 16, 64, 0, 0, 0);
        len      = fb.fb.map_len;
        esperado = malloc(len);

        if (esperado == NULL) {
            printf("FAIL: sin memoria\n");
            exit(1);
        }

        expected_blit(esperado, len, &fb.fb.geom, &surface, &rect);

        BA_CHECK(ba_fb_blit(&fb.fb, &surface, &rect));
        BA_CHECK_MSG(memcmp(fb.fb.mem, esperado, len) == 0, "difiere con line_length + 64");

        {
            int fuera = 0;

            for (i = 0; i < SENTINEL; i++) {
                if (fb.raw[i] != 0x5A || fb.raw[SENTINEL + len + (size_t) i] != 0x5A) {
                    fuera++;
                }
            }

            BA_CHECK_EQ_INT(fuera, 0);
        }

        free(esperado);
        fake_fb_free(&fb);
    }

    /* 32bpp con dos bitfields distintos: la copia tambien es byte a byte la
     * esperada y los colores vuelven al decodificar con los bitfields reales. */
    {
        int variantes[2] = { 1, 2 };
        int v;

        for (v = 0; v < 2; v++) {
            fake_fb_t fb;
            ba_rect_t rect = { 0, 0, 3, 3 };
            uint8_t  *esperado;
            size_t    len;
            int       x, y, ok_colores = 1;

            fake_fb_init(&fb, 32, 0, 0, 0, variantes[v]);
            len      = fb.fb.map_len;
            esperado = malloc(len);

            if (esperado == NULL) {
                printf("FAIL: sin memoria\n");
                exit(1);
            }

            expected_blit(esperado, len, &fb.fb.geom, &surface, &rect);

            BA_CHECK(ba_fb_blit(&fb.fb, &surface, &rect));
            BA_CHECK_MSG(memcmp(fb.fb.mem, esperado, len) == 0, "variante %d difiere", variantes[v]);

            for (y = rect.y1; y <= rect.y2; y++) {
                for (x = rect.x1; x <= rect.x2; x++) {
                    uint32_t px = fake_read_px(&fb, (uint32_t) y, (uint32_t) (BA_LOGICAL_W - 1 - x));
                    uint16_t esperado_px = img[(size_t) y * BA_LOGICAL_W + (size_t) x];
                    uint32_t r5 = ((uint32_t) esperado_px >> 11) & 0x1Fu;
                    uint32_t g6 = ((uint32_t) esperado_px >> 5) & 0x3Fu;
                    uint32_t b5 = (uint32_t) esperado_px & 0x1Fu;
                    int      r  = (int) ((px >> fb.fb.geom.red.offset) & ((1u << fb.fb.geom.red.length) - 1u));
                    int      g  = (int) ((px >> fb.fb.geom.green.offset) & ((1u << fb.fb.geom.green.length) - 1u));
                    int      b  = (int) ((px >> fb.fb.geom.blue.offset) & ((1u << fb.fb.geom.blue.length) - 1u));

                    if (r != (int) ((r5 * 255u + 15u) / 31u) || g != (int) ((g6 * 255u + 31u) / 63u) ||
                        b != (int) ((b5 * 255u + 15u) / 31u)) {
                        ok_colores = 0;
                    }
                }
            }

            BA_CHECK_MSG(ok_colores == 1, "variante %d: los colores no vuelven", variantes[v]);

            free(esperado);
            fake_fb_free(&fb);
        }
    }

    /* Offsets distintos de cero: el blit los respeta y no sale del mapeo. */
    {
        fake_fb_t fb;
        ba_rect_t rect = { 100, 200, 109, 209 };
        uint8_t  *esperado;
        size_t    len;

        fake_fb_init(&fb, 16, 16, 2, 4, 0);
        len      = fb.fb.map_len;
        esperado = malloc(len);

        if (esperado == NULL) {
            printf("FAIL: sin memoria\n");
            exit(1);
        }

        expected_blit(esperado, len, &fb.fb.geom, &surface, &rect);

        BA_CHECK(ba_fb_blit(&fb.fb, &surface, &rect));
        BA_CHECK_MSG(memcmp(fb.fb.mem, esperado, len) == 0, "difiere con xoffset/yoffset");

        free(esperado);
        fake_fb_free(&fb);
    }
}

static void test_blit_invalid(void) {
    fake_fb_t    fb;
    ba_surface_t surface;
    ba_rect_t    fuera     = { BA_LOGICAL_W, BA_LOGICAL_H, BA_LOGICAL_W + 5, BA_LOGICAL_H + 5 };
    ba_rect_t    invertido = { 50, 50, 40, 40 };
    ba_rect_t    parcial   = { -10, -10, 5, 5 };
    size_t       i;
    int          tocado = 0;

    for (i = 0; i < BA_LOGICAL_W * BA_LOGICAL_H; i++) {
        img[i] = BA_COLOR_PRIMARY;
    }

    surface.px = img;
    surface.w  = BA_LOGICAL_W;
    surface.h  = BA_LOGICAL_H;

    fake_fb_init(&fb, 16, 0, 0, 0, 0);

    BA_CHECK(ba_fb_blit(&fb.fb, &surface, &fuera) == false);
    BA_CHECK(ba_fb_blit(&fb.fb, &surface, &invertido) == false);

    for (i = 0; i < fb.len; i++) {
        if (fb.raw[i] != 0x5A) {
            tocado++;
        }
    }

    BA_CHECK_MSG(tocado == 0, "se escribio algo con rects invalidos (%d bytes)", tocado);

    /* Un rect que se sale del surface se recorta: solo se escribe lo valido y
     * nunca fuera del mapeo. */
    {
        uint8_t *esperado = malloc(fb.fb.map_len);

        if (esperado == NULL) {
            printf("FAIL: sin memoria\n");
            exit(1);
        }

        {
            ba_rect_t recortado = { 0, 0, 5, 5 };

            expected_blit(esperado, fb.fb.map_len, &fb.fb.geom, &surface, &recortado);
        }

        BA_CHECK(ba_fb_blit(&fb.fb, &surface, &parcial) == false);
        BA_CHECK_MSG(memcmp(fb.fb.mem, esperado, fb.fb.map_len) == 0, "el recorte no es el esperado");
        free(esperado);
    }

    /* Sin framebuffer mapeado o argumentos nulos: no se escribe nada. */
    {
        ba_fb_t   desconectado;
        ba_rect_t rect = { 0, 0, 10, 10 };

        memset(&desconectado, 0, sizeof(desconectado));
        desconectado.mem      = NULL;
        desconectado.attached = false;

        BA_CHECK(ba_fb_blit(&desconectado, &surface, &rect) == false);
        BA_CHECK(ba_fb_blit(&fb.fb, NULL, &rect) == false);
        BA_CHECK(ba_fb_blit(&fb.fb, &surface, NULL) == false);
    }

    fake_fb_free(&fb);
}

static void test_open_negatives(void) {
    ba_fb_t fb;

    /* Dispositivo ausente: ENOENT, que es el unico caso que el contrato
     * reintenta. */
    BA_CHECK_EQ_INT(ba_fb_open_path("/tmp/x6200-bootanim-no-existe/fb0", &fb), BA_FB_ERR_DEVICE_ABSENT);

    /* Existe pero no es un framebuffer: ioctl falla. */
    {
        char path[] = "/tmp/x6200-bootanim-fbXXXXXX";
        int  fd     = mkstemp(path);

        BA_CHECK(fd >= 0);

        if (fd >= 0) {
            BA_CHECK_EQ_INT(ba_fb_open_path(path, &fb), BA_FB_ERR_IOCTL);
            (void) close(fd);
            (void) unlink(path);
        }
    }

    /* Sin permisos: no se abre. Como root el sistema de archivos no los aplica,
     * asi que ahi se informa el skip con su motivo. */
    if (geteuid() == 0) {
        printf("SKIP: caso 'sin permisos' omitido (uid 0 ignora los permisos del archivo)\n");
    } else {
        char path[] = "/tmp/x6200-bootanim-permXXXXXX";
        int  fd     = mkstemp(path);

        BA_CHECK(fd >= 0);

        if (fd >= 0) {
            BA_CHECK_EQ_INT(chmod(path, 0000), 0);
            (void) close(fd);
            BA_CHECK_EQ_INT(ba_fb_open_path(path, &fb), BA_FB_ERR_DEVICE);
            (void) unlink(path);
        }
    }

    BA_CHECK_EQ_INT(ba_fb_open_path(NULL, &fb), BA_FB_ERR_DEVICE);
    BA_CHECK_EQ_INT(ba_fb_open_path("/dev/null", NULL), BA_FB_ERR_DEVICE);

    ba_fb_detach(NULL);
    memset(&fb, 0, sizeof(fb));
    fb.fd  = -1;
    fb.mem = NULL;
    ba_fb_detach(&fb);
    BA_CHECK_EQ_INT(fb.fd, -1);
}

/* Fixture geometrico de la lluvia (issue #204): las cuatro cajas del contrato,
 * tal como las declara el renderer, se copian al framebuffer fisico byte a byte
 * contra el oraculo y sin escribir nada fuera de ellas (rotacion 90 grados y
 * stride con padding incluidos). */
static void test_rain_boxes(void) {
    ba_surface_t surface;
    fake_fb_t    fb;
    int          i, lane;

    for (i = 0; i < BA_LOGICAL_W * BA_LOGICAL_H; i++) {
        img[i] = (uint16_t) (0x2000u + (uint16_t) (i % 251u));
    }

    surface.px = img;
    surface.w  = BA_LOGICAL_W;
    surface.h  = BA_LOGICAL_H;

    fake_fb_init(&fb, 16, 64, 0, 0, 0);

    {
        size_t  len      = fb.fb.map_len;
        uint8_t *esperado = malloc(len);

        if (esperado == NULL) {
            printf("FAIL: sin memoria\n");
            exit(1);
        }

        for (lane = 0; lane < BA_RAIN_LANES; lane++) {
            ba_rect_t box = ba_rain_box(lane);

            /* Cada lane se prueba por separado: el mapeo vuelve al patron inicial
             * (0x5A) para que la comparacion sea solo de esa caja. */
            memset(fb.fb.mem, 0x5A, len);

            /* Cada lane tiene la geometria exacta del contrato y cabe entera. */
            BA_CHECK_EQ_INT(box.y1, 112);
            BA_CHECK_EQ_INT(box.y2, 347);
            BA_CHECK_EQ_INT(box.x2 - box.x1 + 1, 10);
            BA_CHECK(ba_rect_inside(&box, &(ba_rect_t) { 0, 0, BA_LOGICAL_W - 1, BA_LOGICAL_H - 1 }));

            expected_blit(esperado, len, &fb.fb.geom, &surface, &box);
            BA_CHECK(ba_fb_blit(&fb.fb, &surface, &box));
            BA_CHECK_MSG(memcmp(fb.fb.mem, esperado, len) == 0, "lane %d difiere del oraculo", lane);

            /* Los centinelas de afuera del mapeo siguen intactos. */
            {
                int tocados = 0;

                for (i = 0; i < SENTINEL; i++) {
                    if (fb.raw[i] != 0x5A || fb.raw[SENTINEL + len + (size_t) i] != 0x5A) {
                        tocados++;
                    }
                }

                BA_CHECK_MSG(tocados == 0, "lane %d toco los centinelas (%d bytes)", lane, tocados);
            }

            /* El mapeo fisico del lane es el de LVGL: (x,y) -> (y, 799-x). */
            {
                int32_t fx, fy;

                ba_fb_map_point(box.x1, box.y1, &fx, &fy);
                BA_CHECK_EQ_INT(fx, box.y1);
                BA_CHECK_EQ_INT(fy, BA_LOGICAL_W - 1 - box.x1);

                ba_fb_map_point(box.x2, box.y2, &fx, &fy);
                BA_CHECK_EQ_INT(fx, box.y2);
                BA_CHECK_EQ_INT(fy, BA_LOGICAL_W - 1 - box.x2);
            }
        }

        free(esperado);
    }

    fake_fb_free(&fb);
}

static void test_palette(void) {
    /* Los tres colores del contrato redondeados a 5/6/5. */
    BA_CHECK_EQ_INT(BA_COLOR_BG, 0x0882);        /* #0B1012 */
    BA_CHECK_EQ_INT(BA_COLOR_PRIMARY, 0x8756);   /* #85E9B2 */
    BA_CHECK_EQ_INT(BA_COLOR_SECONDARY, 0x74B0); /* #729784 */
}

int main(void) {
    test_geom_ok();
    test_geom_negatives();
    test_rotation_oracle();
    test_blit_padding_and_formats();
    test_blit_invalid();
    test_open_negatives();
    test_rain_boxes();
    test_palette();
    BA_TEST_SUMMARY("fb_test");
}

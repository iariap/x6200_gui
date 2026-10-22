/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Backend de framebuffer del arranque neo-retro (issue #192).
 *
 *  Rotacion: la GUI corre en 800x480 logico sobre un panel fisico 480x800 con
 *  sw_rotate + LV_DISP_ROT_90 (src/main.c). La rotacion por software de LVGL
 *  8.3.11 (lvgl/src/core/lv_refr.c, draw_buf_rotate_90 + el ajuste del area
 *  para ROT_90) deja el pixel logico (x, y) en el fisico (X = y, Y = 799 - x):
 *    - el eje logico y crece hacia +X (fila por fila del area fisica),
 *    - el eje logico x crece hacia -Y.
 *  El test de host compara esta funcion contra una transcripcion independiente
 *  del algoritmo de LVGL (tests/bootanim/fb_test.c) y los cuatro extremos.
 *
 *  Composicion de color: se usan SIEMPRE los offsets/lengths reales de
 *  fb_var_screeninfo; no se hardcodea RGBA/BGRA ni el orden de bytes.
 */
#define _POSIX_C_SOURCE 200809L /* open/ioctl/mmap con -std=c11 estricto */

#include "fb.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

/* ---------------------------------------------------------------------------
 * Validacion
 * --------------------------------------------------------------------------- */
const char *ba_fb_err_str(ba_fb_err_t err) {
    switch (err) {
        case BA_FB_OK:                 return "ok";
        case BA_FB_ERR_DEVICE:         return "no se pudo abrir el framebuffer";
        case BA_FB_ERR_DEVICE_ABSENT:  return "el framebuffer todavia no existe";
        case BA_FB_ERR_IOCTL:          return "ioctl(FBIOGET_FSCREENINFO/VSCREENINFO) fallo";
        case BA_FB_ERR_RESOLUTION:     return "resolucion fuera del panel 480x800";
        case BA_FB_ERR_BPP:            return "bits por pixel no soportados (16 o 32)";
        case BA_FB_ERR_OFFSET:         return "xoffset/yoffset distintos de cero";
        case BA_FB_ERR_STRIDE:         return "line_length/smem_len inconsistentes";
        case BA_FB_ERR_MEMORY:         return "memoria insuficiente para el panel";
        case BA_FB_ERR_BITFIELDS:      return "bitfields de color invalidos";
        case BA_FB_ERR_MMAP:           return "mmap del framebuffer fallo";
    }

    return "error desconocido";
}

static bool ba_field_ok(const ba_fb_field_t *f, uint32_t limit) {
    return f->length > 0 && f->length <= 8 && (uint32_t) f->offset + (uint32_t) f->length <= limit;
}

static bool ba_field_overlaps(const ba_fb_field_t *a, const ba_fb_field_t *b) {
    uint32_t a1 = (uint32_t) a->offset + (uint32_t) a->length;
    uint32_t b1 = (uint32_t) b->offset + (uint32_t) b->length;

    return (uint32_t) a->offset < b1 && (uint32_t) b->offset < a1;
}

ba_fb_err_t ba_fb_geom_from(const struct fb_fix_screeninfo *fix, const struct fb_var_screeninfo *var,
                            ba_fb_geom_t *out) {
    uint32_t stride_min;
    uint64_t need;

    if (fix == NULL || var == NULL || out == NULL) {
        return BA_FB_ERR_IOCTL;
    }

    memset(out, 0, sizeof(*out));

    if (var->xres != BA_PHYS_W || var->yres != BA_PHYS_H) {
        return BA_FB_ERR_RESOLUTION;
    }

    if (var->bits_per_pixel != 16u && var->bits_per_pixel != 32u) {
        return BA_FB_ERR_BPP;
    }

    /* El panel visible tiene que estar en el origen: con offset, el layout real
       es otro y no hay que adivinar (mejor no dibujar). */
    if (var->xoffset != 0u || var->yoffset != 0u) {
        return BA_FB_ERR_OFFSET;
    }

    out->bytes_per_pixel = (uint8_t) (var->bits_per_pixel / 8u);

    /* line_length puede traer padding, pero no puede ser menor a una fila. */
    stride_min = BA_PHYS_W * (uint32_t) out->bytes_per_pixel;

    if (fix->line_length < stride_min || fix->line_length > 4u * 1024u * 1024u) {
        return BA_FB_ERR_STRIDE;
    }

    /* Tamano de una pantalla completa con el stride real, en 64 bits (sin
       overflow) y despues acotado a lo que el kernel dice que hay. */
    need = (uint64_t) fix->line_length * (uint64_t) BA_PHYS_H;

    if (need > 64u * 1024u * 1024u) {
        return BA_FB_ERR_MEMORY;
    }

    if ((uint64_t) fix->smem_len < need) {
        return BA_FB_ERR_MEMORY;
    }

    out->xres          = var->xres;
    out->yres          = var->yres;
    out->bits_per_pixel = var->bits_per_pixel;
    out->xoffset       = var->xoffset;
    out->yoffset       = var->yoffset;
    out->line_length   = fix->line_length;
    out->smem_len      = (uint32_t) fix->smem_len;
    out->red.offset    = (uint8_t) var->red.offset;
    out->red.length    = (uint8_t) var->red.length;
    out->green.offset  = (uint8_t) var->green.offset;
    out->green.length  = (uint8_t) var->green.length;
    out->blue.offset   = (uint8_t) var->blue.offset;
    out->blue.length   = (uint8_t) var->blue.length;
    out->transp.offset = (uint8_t) var->transp.offset;
    out->transp.length = (uint8_t) var->transp.length;
    out->has_transp    = var->transp.length > 0u;

    if (var->bits_per_pixel == 16u) {
        if (out->red.length != 5u || out->green.length != 6u || out->blue.length != 5u) {
            return BA_FB_ERR_BITFIELDS;
        }
    }

    if (!ba_field_ok(&out->red, var->bits_per_pixel) || !ba_field_ok(&out->green, var->bits_per_pixel) ||
        !ba_field_ok(&out->blue, var->bits_per_pixel)) {
        return BA_FB_ERR_BITFIELDS;
    }

    if (ba_field_overlaps(&out->red, &out->green) || ba_field_overlaps(&out->green, &out->blue) ||
        ba_field_overlaps(&out->red, &out->blue)) {
        return BA_FB_ERR_BITFIELDS;
    }

    if (out->has_transp && (out->transp.offset >= var->bits_per_pixel ||
                            ba_field_overlaps(&out->transp, &out->red) || ba_field_overlaps(&out->transp, &out->green) ||
                            ba_field_overlaps(&out->transp, &out->blue))) {
        return BA_FB_ERR_BITFIELDS;
    }

    return BA_FB_OK;
}

/* ---------------------------------------------------------------------------
 * Apertura / mapeo
 * --------------------------------------------------------------------------- */
ba_fb_err_t ba_fb_open_path(const char *path, ba_fb_t *fb) {
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    ba_fb_err_t              err;
    int                      fd;

    if (fb == NULL || path == NULL) {
        return BA_FB_ERR_DEVICE;
    }

    memset(fb, 0, sizeof(*fb));
    fb->fd  = -1;
    fb->mem = MAP_FAILED;

    fd = open(path, O_RDWR | O_CLOEXEC);

    if (fd < 0) {
        return (errno == ENOENT) ? BA_FB_ERR_DEVICE_ABSENT : BA_FB_ERR_DEVICE;
    }

    memset(&fix, 0, sizeof(fix));
    memset(&var, 0, sizeof(var));

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0 || ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0) {
        close(fd);
        return BA_FB_ERR_IOCTL;
    }

    err = ba_fb_geom_from(&fix, &var, &fb->geom);

    if (err != BA_FB_OK) {
        close(fd);
        return err;
    }

    /* Se mapea lo que el kernel declara mapeable (igual que fbdev_flush de la
       GUI): si eso falla, no se escribe nada. */
    fb->map_len = (size_t) fb->geom.smem_len;
    fb->mem     = mmap(NULL, fb->map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (fb->mem == MAP_FAILED) {
        close(fd);
        fb->fd  = -1;
        fb->mem = NULL;
        return BA_FB_ERR_MMAP;
    }

    fb->fd       = fd;
    fb->attached = true;

    return BA_FB_OK;
}

ba_fb_err_t ba_fb_open(ba_fb_t *fb) {
    return ba_fb_open_path(BA_FB_PATH, fb);
}

void ba_fb_detach(ba_fb_t *fb) {
    if (fb == NULL) {
        return;
    }

    if (fb->mem != NULL && fb->mem != MAP_FAILED && fb->map_len > 0) {
        /* Se suelta el mapeo SIN borrar ni redibujar: el ultimo cuadro queda en
           pantalla hasta que la GUI pinte. */
        munmap(fb->mem, fb->map_len);
    }

    if (fb->fd >= 0) {
        close(fb->fd);
    }

    fb->mem      = NULL;
    fb->fd       = -1;
    fb->map_len  = 0;
    fb->attached = false;
}

/* ---------------------------------------------------------------------------
 * Blit (puro sobre la memoria que recibe)
 * --------------------------------------------------------------------------- */
void ba_fb_map_point(int32_t lx, int32_t ly, int32_t *fx, int32_t *fy) {
    if (fx != NULL) {
        *fx = ly;
    }

    if (fy != NULL) {
        *fy = BA_LOGICAL_W - 1 - lx;
    }
}

/* Un componente de 5/6 bits a la profundidad real del bitfield del kernel. */
static uint32_t ba_scale_component(uint32_t value, uint32_t src_bits, const ba_fb_field_t *dst) {
    uint32_t src_max = (1u << src_bits) - 1u;
    uint32_t dst_max = (1u << dst->length) - 1u;
    uint32_t v8      = (value * 255u + src_max / 2u) / src_max;

    if (dst->length >= 8u) {
        return v8 & 0xFFu;
    }

    return ((v8 * dst_max + 127u) / 255u) & dst_max;
}

static uint32_t ba_pixel_out(const ba_fb_geom_t *g, uint16_t rgb565) {
    uint32_t r5 = ((uint32_t) rgb565 >> 11) & 0x1Fu;
    uint32_t g6 = ((uint32_t) rgb565 >> 5) & 0x3Fu;
    uint32_t b5 = (uint32_t) rgb565 & 0x1Fu;
    uint32_t out = 0;

    out |= ba_scale_component(r5, 5u, &g->red) << g->red.offset;
    out |= ba_scale_component(g6, 6u, &g->green) << g->green.offset;
    out |= ba_scale_component(b5, 5u, &g->blue) << g->blue.offset;

    return out;
}

bool ba_fb_blit(const ba_fb_t *fb, const ba_surface_t *src, const ba_rect_t *rect) {
    int32_t   x1, x2, y1, y2;
    int32_t   lx, ly;
    bool      clipped = false;
    uint8_t   bpp;
    uint64_t  stride;

    if (fb == NULL || src == NULL || src->px == NULL || rect == NULL || !fb->attached || fb->mem == NULL) {
        return false;
    }

    if (!ba_rect_valid(rect)) {
        return false;
    }

    x1 = rect->x1;
    y1 = rect->y1;
    x2 = rect->x2;
    y2 = rect->y2;

    if (x1 < 0 || y1 < 0 || x2 >= src->w || y2 >= src->h) {
        clipped = true;
        if (x1 < 0) {
            x1 = 0;
        }
        if (y1 < 0) {
            y1 = 0;
        }
        if (x2 > src->w - 1) {
            x2 = src->w - 1;
        }
        if (y2 > src->h - 1) {
            y2 = src->h - 1;
        }
    }

    if (x1 > x2 || y1 > y2) {
        return false;
    }

    bpp    = fb->geom.bytes_per_pixel;
    stride = fb->geom.line_length;

    /* Limites fisicos: el rectangulo logico se proyecta en (ly, 799-lx). */
    {
        int32_t fx1, fy1, fx2, fy2;

        ba_fb_map_point(x2, y1, &fx1, &fy1); /* esquina de menor X/Y fisico */
        ba_fb_map_point(x1, y2, &fx2, &fy2); /* esquina de mayor X/Y fisico */

        if (fx1 < 0 || fy1 < 0 || fx2 > (int32_t) fb->geom.xres - 1 || fy2 > (int32_t) fb->geom.yres - 1) {
            clipped = true;
        }

        if (fx1 > fx2 || fy1 > fy2 || fx2 < 0 || fy2 < 0 || fx1 > (int32_t) fb->geom.xres - 1 ||
            fy1 > (int32_t) fb->geom.yres - 1) {
            return false;
        }
    }

    for (ly = y1; ly <= y2; ly++) {
        int32_t fx = ly;

        for (lx = x1; lx <= x2; lx++) {
            int32_t  fy = BA_LOGICAL_W - 1 - lx;
            uint64_t off;
            uint32_t value;
            uint16_t in;

            if (fx < 0 || fy < 0 || fx >= (int32_t) fb->geom.xres || fy >= (int32_t) fb->geom.yres) {
                continue;
            }

            off = (uint64_t) (fy + (int32_t) fb->geom.yoffset) * stride +
                  (uint64_t) (fx + (int32_t) fb->geom.xoffset) * (uint64_t) bpp;

            if (off + bpp > (uint64_t) fb->map_len) {
                clipped = true;
                continue;
            }

            in    = src->px[(size_t) ly * (size_t) src->w + (size_t) lx];
            value = ba_pixel_out(&fb->geom, in);

            /* memcpy: un solo store, sin depender del alineamiento del stride. */
            memcpy((uint8_t *) fb->mem + off, &value, bpp);
        }
    }

    return !clipped;
}

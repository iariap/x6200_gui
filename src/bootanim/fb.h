/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Backend de framebuffer del arranque neo-retro (issue #192): ioctl/mmap/
 *  stride/formato/rotacion. Es el unico modulo que conoce /dev/fb0.
 *
 *  Se separa en dos partes para poder probarlo sin hardware:
 *    - ba_fb_geom_from(): valida fb_fix_screeninfo/fb_var_screeninfo y arma la
 *      geometria (puro, sin tocar el dispositivo);
 *    - ba_fb_blit(): copia un rectangulo logico RGB565 al framebuffer fisico
 *      (puro sobre la memoria que recibe, con bounds check).
 *  El test de host los ejercita con estructuras de ioctl sinteticas y guard
 *  bytes sentinela (tests/bootanim/fb_test.c).
 */
#ifndef X6200_BOOTANIM_FB_H
#define X6200_BOOTANIM_FB_H

#include <linux/fb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Panel fisico del X6200: retrato 480x800; la logica es 800x480. */
#define BA_PHYS_W 480
#define BA_PHYS_H 800

/* Reintentos del contrato (seccion 5): /dev/fb0 puede no estar listo todavia;
   20 intentos separados 100 ms, en el proceso hijo, maximo 2 s. */
#define BA_FB_TRIES     20
#define BA_FB_RETRY_MS  100

#define BA_FB_PATH "/dev/fb0"

typedef enum {
    BA_FB_OK = 0,
    BA_FB_ERR_DEVICE,        /* no se pudo abrir /dev/fb0 */
    BA_FB_ERR_DEVICE_ABSENT, /* /dev/fb0 todavia no existe: el contrato reintenta */
    BA_FB_ERR_IOCTL,         /* FBIOGET_FSCREENINFO / FBIOGET_VSCREENINFO */
    BA_FB_ERR_RESOLUTION,
    BA_FB_ERR_BPP,
    BA_FB_ERR_OFFSET,
    BA_FB_ERR_STRIDE,
    BA_FB_ERR_MEMORY,
    BA_FB_ERR_BITFIELDS,
    BA_FB_ERR_MMAP,
} ba_fb_err_t;

typedef struct {
    uint8_t  offset;
    uint8_t  length;
} ba_fb_field_t;

typedef struct {
    uint32_t     xres, yres;        /* resolucion VISIBLE del panel */
    uint32_t     bits_per_pixel;
    uint32_t     xoffset, yoffset;  /* deben ser 0 (se rechaza otra cosa) */
    uint32_t     line_length;       /* bytes por linea, con padding */
    uint32_t     smem_len;          /* bytes mapeables */
    uint8_t      bytes_per_pixel;
    ba_fb_field_t red, green, blue, transp;
    bool         has_transp;
} ba_fb_geom_t;

typedef struct {
    int            fd;
    void          *mem;
    size_t         map_len;
    bool           attached;
    ba_fb_geom_t   geom;
} ba_fb_t;

/* Valida la geometria del panel. Devuelve BA_FB_OK o el motivo del rechazo. */
ba_fb_err_t ba_fb_geom_from(const struct fb_fix_screeninfo *fix, const struct fb_var_screeninfo *var,
                            ba_fb_geom_t *out);
const char *ba_fb_err_str(ba_fb_err_t err);

/* Abre, valida y mapea. Devuelve BA_FB_OK y deja `fb` listo, o el error (con el
   fd cerrado y sin mapear nada). Nunca escribe en el framebuffer. */
ba_fb_err_t ba_fb_open(ba_fb_t *fb);
ba_fb_err_t ba_fb_open_path(const char *path, ba_fb_t *fb); /* seam de test: path explicito */
void        ba_fb_detach(ba_fb_t *fb);                      /* munmap + close, sin tocar el dibujo */

/* Copia el rectangulo logico `rect` del surface al framebuffer fisico, rotado 90
   grados igual que la GUI (LV_DISP_ROT_90). Devuelve true si escribio el
   rectangulo completo; false si tuvo que recortarlo (nunca escribe fuera de
   limites ni fuera de la memoria mapeada). */
bool ba_fb_blit(const ba_fb_t *fb, const ba_surface_t *src, const ba_rect_t *rect);

/* Rotacion logica -> fisica (contrato de LVGL, ver renderer/fb.c): el pixel
   logico (x,y) cae en el fisico (y, BA_LOGICAL_W-1-x). Se expone para que los
   tests la comparen contra el oraculo independiente. */
void ba_fb_map_point(int32_t lx, int32_t ly, int32_t *fx, int32_t *fy);

#ifdef __cplusplus
}
#endif

#endif /* X6200_BOOTANIM_FB_H */

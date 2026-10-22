/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Volcado del framebuffer fisico esperado (issue #192; ciclo continuo del
 *  issue #204): reproduce el ciclo completo con el renderer y el backend REALES
 *  y el MISMO ciclo que src/bootanim/main.c (fondo completo en el primer cuadro
 *  y despues solo las cajas que cambiaron), sobre un buffer con el
 *  stride/formato pedidos. El ciclo es continuo: `last` decide cuantos cuadros
 *  se reproducen (los 30 s ya no terminan nada).
 *
 *  El gate compara byte a byte este volcado con lo que dejo el binario
 *  x6200_bootanim compilado con -DBOOTANIM_TEST (reloj sintetico + limite de
 *  cuadros): eso prueba, de una sola vez, tiempos, rasterizado, rotacion,
 *  formato, stride y limites.
 *
 *  Uso: replay_dump <out> <bpp> <pad> <xoff> <yoff> <rgb|bgr> [ultimo_cuadro]
 */
#include "fb.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_int(const char *s) {
    return (int) strtol(s, NULL, 10);
}

int main(int argc, char **argv) {
    const char *out_path;
    int         bpp, pad, xoff, yoff, last = 240;
    int         bgr;
    uint32_t    line;
    size_t      len;
    uint8_t    *raw;
    ba_fb_t     fb;
    ba_surface_t surface;
    uint32_t    frame;
    ba_model_t  prev;
    int         have_prev = 0;
    FILE       *out;

    if (argc < 7) {
        fprintf(stderr, "uso: %s <out> <bpp> <pad> <xoff> <yoff> <rgb|bgr> [ultimo_cuadro]\n", argv[0]);
        return 2;
    }

    out_path = argv[1];
    bpp      = parse_int(argv[2]);
    pad      = parse_int(argv[3]);
    xoff     = parse_int(argv[4]);
    yoff     = parse_int(argv[5]);
    bgr      = strcmp(argv[6], "bgr") == 0;

    if (argc > 7) {
        last = parse_int(argv[7]);
    }

    line = (uint32_t) BA_PHYS_W * (uint32_t) (bpp / 8) + (uint32_t) (pad < 0 ? 0 : pad);
    len  = (size_t) line * BA_PHYS_H;

    raw = malloc(len);

    if (raw == NULL) {
        fprintf(stderr, "replay_dump: sin memoria (%zu bytes)\n", len);
        return 1;
    }

    /* El archivo que hace de framebuffer arranca con este mismo patron (el gate
     * inicializa el archivo igual): el padding tiene que quedar intacto. */
    memset(raw, 0x5A, len);

    memset(&fb, 0, sizeof(fb));
    fb.mem      = raw;
    fb.map_len  = len;
    fb.attached = true;
    fb.geom.xres            = BA_PHYS_W;
    fb.geom.yres            = BA_PHYS_H;
    fb.geom.bits_per_pixel  = (uint32_t) bpp;
    fb.geom.bytes_per_pixel = (uint8_t) (bpp / 8);
    fb.geom.line_length     = line;
    fb.geom.smem_len        = (uint32_t) len;
    fb.geom.xoffset         = (uint32_t) xoff;
    fb.geom.yoffset         = (uint32_t) yoff;

    if (bpp == 32) {
        if (bgr) {
            fb.geom.red.offset   = 0;
            fb.geom.green.offset = 8;
            fb.geom.blue.offset  = 16;
        } else {
            fb.geom.red.offset   = 16;
            fb.geom.green.offset = 8;
            fb.geom.blue.offset  = 0;
        }

        fb.geom.red.length   = 8;
        fb.geom.green.length = 8;
        fb.geom.blue.length  = 8;
    } else {
        fb.geom.red.offset   = 11;
        fb.geom.green.offset = 5;
        fb.geom.blue.offset  = 0;
        fb.geom.red.length   = 5;
        fb.geom.green.length = 6;
        fb.geom.blue.length  = 5;
    }

    surface.px = malloc((size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t));

    if (surface.px == NULL) {
        fprintf(stderr, "replay_dump: sin memoria para el surface\n");
        free(raw);
        return 1;
    }

    surface.w = BA_LOGICAL_W;
    surface.h = BA_LOGICAL_H;

    /* El mismo ciclo que src/bootanim/main.c: ba_model_at + ba_render + blit de
     * los rectangulos sucios, con el fondo completo solo en el primer cuadro y
     * la animacion (lluvia, beams, decrypt) viva sin importar el tiempo. */
    for (frame = 0; frame <= (uint32_t) last; frame++) {
        ba_model_t m;
        ba_dirty_t dirty;
        int        i;

        ba_model_at(frame, (uint64_t) frame * BA_FRAME_MS, &m);
        ba_render(&surface, &m, have_prev ? &prev : NULL, !have_prev, &dirty);

        for (i = 0; i < dirty.count; i++) {
            (void) ba_fb_blit(&fb, &surface, &dirty.rect[i]);
        }

        prev      = m;
        have_prev = 1;
    }

    out = fopen(out_path, "wb");

    if (out == NULL) {
        fprintf(stderr, "replay_dump: no puedo escribir %s\n", out_path);
        free(raw);
        free(surface.px);
        return 1;
    }

    if (fwrite(raw, 1, len, out) != len) {
        fprintf(stderr, "replay_dump: escritura corta en %s\n", out_path);
        (void) fclose(out);
        free(raw);
        free(surface.px);
        return 1;
    }

    (void) fclose(out);
    free(raw);
    free(surface.px);

    return 0;
}

/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Mediciones reales en host del ciclo continuo del arranque (issue #204,
 *  seccion 5): por fase (intro, lluvia, BEAMS, DECRYPT, pie demorado) imprime
 *  cuantos cuadros entran, los pixeles sucios (promedio y maximo, en % de
 *  800x480), los bytes que se copiarian al framebuffer a 16 bpp y el tiempo de
 *  render por cuadro. No es un gate: no tiene umbrales, solo numeros medidos
 *  (los umbrales los verifican los tests).
 *
 *  Uso: render_bench [cuadros]   (default 2200 = 184,8 s de animacion)
 */
#define _POSIX_C_SOURCE 200809L

#include "fb.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    FASE_INTRO = 0,
    FASE_FIRST,
    FASE_RAIN,
    FASE_BEAMS,
    FASE_DECRYPT,
    FASE_DELAYED,
    FASE_COUNT
} fase_t;

static const char *fase_name(fase_t f) {
    switch (f) {
        case FASE_INTRO: return "intro";
        case FASE_FIRST: return "primer cuadro (full)";
        case FASE_RAIN: return "lluvia";
        case FASE_BEAMS: return "beams";
        case FASE_DECRYPT: return "decrypt";
        case FASE_DELAYED: return "delayed (lluvia, >30s)";
        default: return "?";
    }
}

typedef struct {
    long cuadros;
    long dirty_px;
    long dirty_px_max;
    long bytes;
    double ns;
} acum_t;

static long dirty_area_px(const ba_dirty_t *d) {
    long total = 0;
    int  i;

    for (i = 0; i < d->count; i++) {
        total += (long) (d->rect[i].x2 - d->rect[i].x1 + 1) * (long) (d->rect[i].y2 - d->rect[i].y1 + 1);
    }

    return total;
}

static double now_ns(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }

    return (double) ts.tv_sec * 1e9 + (double) ts.tv_nsec;
}

int main(int argc, char **argv) {
    uint32_t    total   = 2200;
    uint16_t   *px      = malloc((size_t) BA_LOGICAL_W * BA_LOGICAL_H * sizeof(uint16_t));
    ba_surface_t surface;
    acum_t      acum[FASE_COUNT];
    ba_model_t  prev;
    int         have_prev = 0;
    uint32_t    frame;
    long        overflow  = 0;
    long        max_count = 0;

    if (px == NULL) {
        fprintf(stderr, "render_bench: sin memoria\n");
        return 1;
    }

    if (argc > 1) {
        total = (uint32_t) strtoul(argv[1], NULL, 10);
    }

    surface.px = px;
    surface.w  = BA_LOGICAL_W;
    surface.h  = BA_LOGICAL_H;

    memset(acum, 0, sizeof(acum));

    for (frame = 0; frame <= total; frame++) {
        ba_model_t m;
        ba_dirty_t d;
        double     t0, t1;
        long       area;
        fase_t     f;

        ba_model_at(frame, (uint64_t) frame * BA_FRAME_MS, &m);
        t0 = now_ns();
        ba_render(&surface, &m, have_prev ? &prev : NULL, !have_prev, &d);
        t1 = now_ns();

        if (d.overflow) {
            overflow++;
        }

        if (d.count > max_count) {
            max_count = d.count;
        }

        if (m.beam_cell >= 0) {
            f = FASE_BEAMS;
        } else if (!m.rain_active) {
            f = (frame == 0 && !have_prev) ? FASE_FIRST : FASE_INTRO;
        } else if (m.delayed) {
            f = FASE_DELAYED;
        } else {
            /* DECRYPT: alguna posicion no fija despues de la intro. */
            int pos, unlocked = 0;

            for (pos = 0; pos < BA_LOGO_LEN; pos++) {
                if (!m.logo_locked[pos]) {
                    unlocked++;
                }
            }

            f = unlocked > 0 ? FASE_DECRYPT : FASE_RAIN;
        }

        area = dirty_area_px(&d);

        acum[f].cuadros++;
        acum[f].dirty_px += area;
        acum[f].bytes += area * 2; /* RGB565 -> 2 bytes por pixel */
        acum[f].ns += t1 - t0;

        if (area > acum[f].dirty_px_max) {
            acum[f].dirty_px_max = area;
        }

        prev      = m;
        have_prev = 1;
    }

    printf("render_bench: %u cuadros (%llu ms de animacion), pantalla %dx%d\n", total + 1u,
           (unsigned long long) (uint64_t) total * BA_FRAME_MS, BA_LOGICAL_W, BA_LOGICAL_H);
    printf("presupuesto: BA_FRAME_MS=%ums, BA_MAX_DIRTY=%d, max dirty.count observado=%ld, overflow=%ld\n",
           BA_FRAME_MS, BA_MAX_DIRTY, max_count, overflow);
    printf("\n%-22s %8s %12s %12s %12s %12s %14s\n", "fase", "cuadros", "dirty px avg", "dirty px max",
           "dirty % max", "bytes/cuadro", "render us/cuadro");

    {
        int f;

        for (f = 0; f < FASE_COUNT; f++) {
            if (acum[f].cuadros == 0) {
                continue;
            }

            printf("%-22s %8ld %12ld %12ld %11.2f%% %12ld %14.1f\n", fase_name((fase_t) f), acum[f].cuadros,
                   acum[f].dirty_px / acum[f].cuadros, acum[f].dirty_px_max,
                   100.0 * (double) acum[f].dirty_px_max / (double) (BA_LOGICAL_W * BA_LOGICAL_H),
                   acum[f].bytes / acum[f].cuadros, acum[f].ns / (double) acum[f].cuadros / 1000.0);
        }
    }

    printf("\n(pixeles sucios = suma de las areas de dirty; bytes/cuadro = esos pixeles a 16 bpp;\n");
    printf(" render us/cuadro = modelo + raster puro, sin blit ni hardware; RAM del raster: 1 buffer fijo)\n");

    free(px);

    return overflow == 0 ? 0 : 1;
}

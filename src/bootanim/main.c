/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  x6200_bootanim: dibujante temprano del arranque (issue #192; ciclo continuo
 *  del issue #204).
 *
 *  Ocupa /dev/fb0 desde el primer momento seguro en userspace y lo suelta cuando
 *  la GUI se lo pide (src/boot_splash.c, justo antes del primer
 *  lv_timer_handler). El ciclo (intro, lluvia, BEAMS, DECRYPT) es continuo hasta
 *  esa cesion: a los 30 s SOLO cambia el texto del pie a `> STARTUP DELAYED_` y
 *  la animacion sigue; el proceso nunca termina solo.
 *
 *  Limites del contrato (seccion 3): sin LVGL, sin radio, sin red, sin shell,
 *  sin GPIO, sin /mnt/params.db, sin threads ni subprocesos. Los handlers de
 *  SIGUSR1/SIGTERM solo escriben una bandera volatile sig_atomic_t (nada de
 *  llamadas no async-signal-safe) y el ciclo la mira antes de cada cuadro. La
 *  salida cierra mmap/fd y NO borra el ultimo cuadro.
 */
#define _POSIX_C_SOURCE 200809L /* sigaction/clock_nanosleep con -std=c11 estricto */

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "fb.h"
#include "renderer.h"

/* Identidad de las FUENTES embebida igual que en la GUI (issue #165): permite
 * saber que codigo corrio sin depender del md5 del binario. */
#ifndef X6200_FUENTE_MD5
#define X6200_FUENTE_MD5 "desconocido"
#endif

/* Un solo escritor visible del framebuffer (mientras el proceso vive). */
static uint16_t ba_surface_px[BA_LOGICAL_W * BA_LOGICAL_H];

static volatile sig_atomic_t ba_stop = 0;

/* ---------------------------------------------------------------------------
 * Banco de host (tests/check_bootanim.sh): con -DBOOTANIM_TEST el ciclo usa un
 * reloj SINTETICO (cada iteracion avanza exactamente BA_FRAME_MS, sin reloj real
 * ni nanosleep) y presenta hasta el cuadro BA_TEST_FRAME_LIMIT, para que el
 * cuadro N sea reproducible byte a byte contra tests/bootanim/replay_dump.c.
 *
 * El macro lo define SOLO el gate del host: la ROM y el CMake de produccion no
 * lo definen (el gate lo verifica sobre el preprocesado), asi que el splash real
 * no tiene ningun flag, env ni condicion que lo detenga antes de la cesion.
 * --------------------------------------------------------------------------- */
#ifdef BOOTANIM_TEST
#ifndef BA_TEST_FRAME_LIMIT
#define BA_TEST_FRAME_LIMIT 600u
#endif
static uint32_t ba_test_frame = 0;
#endif

static void ba_on_signal(int sig) {
    (void) sig; /* async-signal-safe: solo la bandera */
    ba_stop = 1;
}

static void ba_install_signals(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = ba_on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* sin SA_RESTART: clock_nanosleep corta y el ciclo ve la bandera */

    (void) sigaction(SIGUSR1, &sa, NULL);
    (void) sigaction(SIGTERM, &sa, NULL);
}

#ifndef BOOTANIM_TEST
static uint64_t ba_elapsed_ms(const struct timespec *t0) {
    struct timespec now;
    time_t          sec;
    long            nsec;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }

    sec  = now.tv_sec - t0->tv_sec;
    nsec = now.tv_nsec - t0->tv_nsec;

    while (nsec < 0) {
        nsec += 1000000000L;
        sec--;
    }

    if (sec < 0) {
        return 0;
    }

    return (uint64_t) sec * 1000u + (uint64_t) nsec / 1000000u;
}
#endif /* !BOOTANIM_TEST */

/* Reloj del ciclo: monotono real (o sintetico, solo bajo -DBOOTANIM_TEST). */
static uint64_t ba_now_ms(const struct timespec *t0) {
#ifdef BOOTANIM_TEST
    (void) t0;

    return (uint64_t) ba_test_frame * BA_FRAME_MS;
#else
    return ba_elapsed_ms(t0);
#endif
}

/* Espera relativa (reintentos de /dev/fb0): nada de busy-wait y sale enseguida
 * si llega una senal. */
static void ba_sleep_ms(uint32_t ms) {
    struct timespec req;

    req.tv_sec  = (time_t) (ms / 1000u);
    req.tv_nsec = (long) (ms % 1000u) * 1000000L;

    while (!ba_stop) {
        int rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL);

        if (rc == 0) {
            return;
        }

        if (rc != EINTR) {
            return;
        }

        if (req.tv_sec == 0 && req.tv_nsec == 0) {
            return;
        }
    }
}

/* Espera absoluta: los cuadros quedan clavados en la grilla de 12 fps (sin
 * acumular deriva) y una senal corta la espera. En el banco de host no existe:
 * ahi el reloj es sintetico y no se espera nada. */
#ifndef BOOTANIM_TEST
static void ba_wait_until(const struct timespec *t0, uint32_t offset_ms) {
    struct timespec ts = *t0;

    ts.tv_sec += (time_t) (offset_ms / 1000u);
    ts.tv_nsec += (long) (offset_ms % 1000u) * 1000000L;

    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    while (!ba_stop) {
        int rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);

        if (rc != EINTR) {
            return;
        }
    }
}
#endif /* !BOOTANIM_TEST */

/* Rasteriza el cuadro al buffer logico y copia al framebuffer SOLO los
 * rectangulos que cambiaron (el fondo completo, una sola vez, en el primero). */
static void ba_present(ba_fb_t *fb, const ba_model_t *m, const ba_model_t *prev, bool full) {
    static ba_surface_t surface;
    ba_dirty_t          dirty;
    int                 i;

    surface.px = ba_surface_px;
    surface.w  = BA_LOGICAL_W;
    surface.h  = BA_LOGICAL_H;

    ba_render(&surface, m, prev, full, &dirty);

    for (i = 0; i < dirty.count; i++) {
        (void) ba_fb_blit(fb, &surface, &dirty.rect[i]);
    }
}

static bool ba_open_fb(ba_fb_t *fb, ba_fb_err_t *last_err) {
    int i;

    for (i = 0; i < BA_FB_TRIES; i++) {
        ba_fb_err_t err = ba_fb_open(fb);

        if (err == BA_FB_OK) {
            return true;
        }

        *last_err = err;

        if (err != BA_FB_ERR_DEVICE_ABSENT || ba_stop) {
            return false;
        }

        if (i + 1 < BA_FB_TRIES) {
            ba_sleep_ms(BA_FB_RETRY_MS);
        }
    }

    return false;
}

int main(void) {
    ba_fb_t         fb;
    ba_fb_err_t     last_err = BA_FB_ERR_DEVICE_ABSENT;
    struct timespec t0;
    ba_model_t      prev;
    bool            have_prev = false;
    uint32_t        last_frame = 0;

    ba_install_signals();

    if (!ba_open_fb(&fb, &last_err)) {
        /* Degradado y silencioso si el framebuffer no existe: el arranque normal
         * sigue igual y no se deja ninguna traza (el panel no se toca). Otros
         * fallos (formato/mmap/permisos) se dejan en stderr. */
        if (last_err != BA_FB_ERR_DEVICE_ABSENT && !ba_stop) {
            fprintf(stderr, "x6200_bootanim: sin animacion (%s)\n", ba_fb_err_str(last_err));
        }

        return 0;
    }

    /* Ya con el framebuffer tomado: una linea con el sello de las fuentes que se
     * compilaron (issue #165), para poder saber que codigo corrio. */
    fprintf(stderr, "x6200_bootanim: fuente %s\n", X6200_FUENTE_MD5);

    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        ba_fb_detach(&fb);
        return 0;
    }

    while (!ba_stop) {
        uint64_t   elapsed = ba_now_ms(&t0);
        uint32_t   frame   = (uint32_t) (elapsed / BA_FRAME_MS);
        ba_model_t model;

        ba_model_at(frame, elapsed, &model);

        /* Nunca mas de un cuadro por ventana de 84 ms (12 fps maximo): si el
         * reloj ya paso a la ventana siguiente, ese cuadro se saltea. A los 30 s
         * solo cambia el texto del pie: el ciclo sigue hasta ba_stop. */
        if (!have_prev || frame != last_frame) {
            ba_present(&fb, &model, have_prev ? &prev : NULL, !have_prev);
            prev       = model;
            have_prev  = true;
            last_frame = frame;
        }

#ifdef BOOTANIM_TEST
        /* Banco de host: termina solo, en el cuadro pedido (el ciclo de
         * produccion NO tiene esta salida). */
        if (frame >= BA_TEST_FRAME_LIMIT) {
            break;
        }

        ba_test_frame++;
#else
        ba_wait_until(&t0, (frame + 1u) * BA_FRAME_MS);
#endif
    }

    ba_fb_detach(&fb);

    return 0;
}

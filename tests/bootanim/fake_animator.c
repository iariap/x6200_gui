/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Animador falso para los tests de host (issue #192): no dibuja nada, solo se
 *  queda vivo hasta que lo matan. Sirve para probar de verdad la cesion del
 *  framebuffer (src/boot_splash.c) y el singleton de S01bootanim.
 *
 *  Uso: fake_animator [--ignore-usr1] [--ignore-term]
 *    --ignore-usr1   ignora SIGUSR1 (para forzar el camino de SIGTERM)
 *    --ignore-term   ignora SIGTERM (para forzar el camino de SIGKILL)
 */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <string.h>
#include <time.h>

static void on_signal(int sig) {
    (void) sig; /* handler vacio: ignora la senal a proposito */
}

int main(int argc, char **argv) {
    int i;
    int ignore_usr1 = 0;
    int ignore_term = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ignore-usr1") == 0) {
            ignore_usr1 = 1;
        } else if (strcmp(argv[i], "--ignore-term") == 0) {
            ignore_term = 1;
        }
    }

    {
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = on_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        if (ignore_usr1) {
            (void) sigaction(SIGUSR1, &sa, NULL);
        }

        if (ignore_term) {
            (void) sigaction(SIGTERM, &sa, NULL);
        }
    }

    for (;;) {
        struct timespec ts;

        ts.tv_sec  = 30;
        ts.tv_nsec = 0;
        (void) nanosleep(&ts, NULL);
    }

    return 0;
}

/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Cesion del framebuffer del arranque neo-retro (issue #192): implementacion.
 *  Sin LVGL, sin heap y sin estado global mas que los dos paths (ver
 *  src/boot_splash.h).
 */
#define _POSIX_C_SOURCE 200809L /* sigaction/readlink/clock_nanosleep con -std=c11 */

#include "boot_splash.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BS_PIDFILE_DEFAULT "/var/run/x6200_bootanim.pid"
#define BS_EXE_DEFAULT     "/usr/sbin/x6200_bootanim"
#define BS_PATH_MAX        255
#define BS_PID_MAX_LEN     10 /* un pid de Linux no pasa de 7 digitos */

static char bs_pidfile[BS_PATH_MAX + 1] = BS_PIDFILE_DEFAULT;
static char bs_exe[BS_PATH_MAX + 1]     = BS_EXE_DEFAULT;

#ifdef BOOTANIM_TEST
/* Instrumentacion del banco (ver boot_splash.h): contadores de sondeos. Solo
 * bajo -DBOOTANIM_TEST; en produccion no existen. */
static unsigned int bs_test_waits;
static unsigned int bs_test_waited_ms;

void boot_splash_test_reset_waits(void) {
    bs_test_waits     = 0;
    bs_test_waited_ms = 0;
}

unsigned int boot_splash_test_waits(void) {
    return bs_test_waits;
}

unsigned int boot_splash_test_waited_ms(void) {
    return bs_test_waited_ms;
}
#endif /* BOOTANIM_TEST */

void boot_splash_set_paths(const char *pidfile, const char *exe) {
    if (pidfile != NULL) {
        (void) snprintf(bs_pidfile, sizeof(bs_pidfile), "%s", pidfile);
    }

    if (exe != NULL) {
        (void) snprintf(bs_exe, sizeof(bs_exe), "%s", exe);
    }
}

static void bs_sleep_ms(unsigned int ms) {
    struct timespec req;

#ifdef BOOTANIM_TEST
    /* Instrumentacion del banco: el test afirma cuantas veces sono la espera,
     * no cuanto tardo (ver boot_splash.h). En produccion no queda nada. */
    bs_test_waits++;
    bs_test_waited_ms += ms;
#endif

    req.tv_sec  = (time_t) (ms / 1000u);
    req.tv_nsec = (long) (ms % 1000u) * 1000000L;

    while (clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL) == EINTR) {
        /* clock_nanosleep relativo deja en `req` lo que falta. */
        if (req.tv_sec == 0 && req.tv_nsec == 0) {
            break;
        }
    }
}

/* PIDFILE ausente, vacio, con basura o con un valor absurdo: no hay animador al
 * que senalizar (nunca se inventa un PID). */
static bool bs_read_pid(pid_t *pid) {
    char   line[64];
    FILE  *f = fopen(bs_pidfile, "r");
    size_t n, i;
    long   value;

    if (f == NULL) {
        return false;
    }

    if (fgets(line, sizeof(line), f) == NULL) {
        (void) fclose(f);
        return false;
    }

    (void) fclose(f);

    n = strcspn(line, "\r\n");
    line[n] = '\0';

    if (n == 0 || n > BS_PID_MAX_LEN) {
        return false;
    }

    for (i = 0; i < n; i++) {
        if (line[i] < '0' || line[i] > '9') {
            return false;
        }
    }

    value = strtol(line, NULL, 10);

    if (value <= 1) { /* 0, 1 y negativos jamas se senalizan */
        return false;
    }

    *pid = (pid_t) value;

    return true;
}

/* Un proceso zombi ya no escribe el framebuffer: cuenta como terminado. */
static bool bs_alive(pid_t pid) {
    char  path[64];
    char  buf[512];
    FILE *f;

    (void) snprintf(path, sizeof(path), "/proc/%ld/stat", (long) pid);

    f = fopen(path, "r");

    if (f == NULL) {
        return false;
    }

    if (fgets(buf, sizeof(buf), f) == NULL) {
        (void) fclose(f);
        return false;
    }

    (void) fclose(f);

    {
        /* El comm puede tener ')' adentro: se busca el ULTIMO ')'. */
        const char *p = strrchr(buf, ')');

        if (p == NULL || p[1] != ' ' || p[2] == '\0') {
            return true; /* no se pudo interpretar: conservador, se considera vivo */
        }

        return p[2] != 'Z';
    }
}

/* Identidad por /proc/<pid>/exe: es el mismo criterio que usa S01bootanim. */
static bool bs_exe_matches(pid_t pid) {
    char    path[64];
    char    target[BS_PATH_MAX + 1];
    ssize_t n;
    size_t  tl, sl;
    const char *suffix = " (deleted)";

    (void) snprintf(path, sizeof(path), "/proc/%ld/exe", (long) pid);

    n = readlink(path, target, sizeof(target) - 1);

    if (n <= 0) {
        return false;
    }

    target[n] = '\0';

    /* Un ejecutable reemplazado en caliente queda como "<path> (deleted)". */
    tl = strlen(target);
    sl = strlen(suffix);

    if (tl > sl && strcmp(target + tl - sl, suffix) == 0) {
        target[tl - sl] = '\0';
    }

    return strcmp(target, bs_exe) == 0;
}

static bool bs_wait_gone(pid_t pid, unsigned int max_ms) {
    unsigned int waited = 0;

    for (;;) {
        if (!bs_alive(pid)) {
            return true;
        }

        if (waited >= max_ms) {
            return false;
        }

        {
            unsigned int step = max_ms - waited < BOOT_SPLASH_POLL_MS ? max_ms - waited : BOOT_SPLASH_POLL_MS;

            bs_sleep_ms(step);
            waited += step;
        }
    }
}

bool boot_splash_stop(void) {
    pid_t pid;

    if (!bs_read_pid(&pid)) {
        return true; /* sin PIDFILE o PID invalido: no hay nada que detener */
    }

    if (!bs_alive(pid)) {
        return true; /* ya termino (p. ej. se cumplieron los 30 s) */
    }

    if (!bs_exe_matches(pid)) {
        fprintf(stderr, "boot_splash: pid %ld del PIDFILE no es %s: no se senaliza\n", (long) pid, bs_exe);
        return true;
    }

    if (kill(pid, SIGUSR1) != 0) {
        fprintf(stderr, "boot_splash: SIGUSR1 al pid %ld fallo\n", (long) pid);
    }

    if (bs_wait_gone(pid, BOOT_SPLASH_USR1_WAIT_MS)) {
        return true;
    }

    /* Re-verificacion: el PID pudo reciclarse durante la espera. */
    if (!bs_alive(pid) || !bs_exe_matches(pid)) {
        return true;
    }

    (void) kill(pid, SIGTERM);

    if (bs_wait_gone(pid, BOOT_SPLASH_TERM_WAIT_MS)) {
        return true;
    }

    if (!bs_alive(pid) || !bs_exe_matches(pid)) {
        return true;
    }

    (void) kill(pid, SIGKILL);

    if (bs_wait_gone(pid, BOOT_SPLASH_KILL_WAIT_MS)) {
        return true;
    }

    fprintf(stderr, "boot_splash: no se pudo confirmar la salida del animador (pid %ld)\n", (long) pid);

    return false;
}

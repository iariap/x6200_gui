/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Tests de host de la cesion del framebuffer (issue #192, seccion 7):
 *  PIDFILE ausente/vacio/corrupto, PID ajeno o reciclado, proceso ya terminado,
 *  espera de 100 ms + 20 ms, SIGTERM y SIGKILL de ultimo recurso. Se usan
 *  procesos REALES y la identidad se verifica por /proc/<pid>/exe, igual que en
 *  la radio.
 *
 *  Uso: handoff_test <fake_animator>
 */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700 /* realpath */

#include "boot_splash.h"
#include "test_util.h"

#include <errno.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static char animator[4096]; /* path del fake_animator, resuelto */
static char pidfile[128];
static char tmpdir[64];

static uint64_t now_ms(void) {
    struct timespec ts;

    (void) clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t) ts.tv_sec * 1000u + (uint64_t) ts.tv_nsec / 1000000u;
}

static void pidfile_write(const char *text) {
    FILE *f = fopen(pidfile, "w");

    if (f == NULL) {
        printf("FAIL: no puedo escribir %s\n", pidfile);
        exit(1);
    }

    if (text != NULL) {
        fprintf(f, "%s", text);
    }

    (void) fclose(f);
}

static void pidfile_remove(void) {
    (void) unlink(pidfile);
}

static pid_t spawn_animator2(const char *arg, const char *arg2) {
    pid_t pid = fork();

    if (pid == 0) {
        if (arg2 != NULL) {
            execl(animator, animator, arg, arg2, (char *) NULL);
        } else if (arg != NULL) {
            execl(animator, animator, arg, (char *) NULL);
        } else {
            execl(animator, animator, (char *) NULL);
        }

        _exit(127);
    }

    /* Esperar a que el exec termine: la identidad se lee de /proc/<pid>/exe, y
     * antes del exec ahi figura el ejecutable del test. */
    {
        char path[64];
        int  i;

        (void) snprintf(path, sizeof(path), "/proc/%ld/exe", (long) pid);

        for (i = 0; i < 400; i++) {
            char target[4096];
            ssize_t n = readlink(path, target, sizeof(target) - 1);

            if (n > 0) {
                target[n] = '\0';

                if (strcmp(target, animator) == 0) {
                    break;
                }
            }

            {
                struct timespec ts = { 0, 2000000 }; /* 2 ms */

                (void) nanosleep(&ts, NULL);
            }
        }
    }

    return pid;
}

static pid_t spawn_animator(const char *arg) {
    return spawn_animator2(arg, NULL);
}

/* waitpid con techo: si el hijo no termina, se lo mata y se informa -1 (nunca
 * se cuelga el banco de pruebas por un bug). */
static int reap_status(pid_t pid) {
    int status = 0;
    int i;

    for (i = 0; i < 2000; i++) {
        pid_t r = waitpid(pid, &status, WNOHANG);

        if (r == pid) {
            return status;
        }

        if (r < 0 && errno != EINTR) {
            break;
        }

        {
            struct timespec ts = { 0, 1000000 }; /* 1 ms */

            (void) nanosleep(&ts, NULL);
        }
    }

    (void) kill(pid, SIGKILL);
    (void) waitpid(pid, &status, 0);

    return -1;
}

static int child_alive(pid_t pid) {
    return kill(pid, 0) == 0;
}

static void kill_and_reap(pid_t pid) {
    (void) kill(pid, SIGKILL);
    (void) reap_status(pid);
}

/* --- Casos ------------------------------------------------------------------ */

static void test_no_pidfile(void) {
    uint64_t t0;

    pidfile_remove();
    boot_splash_set_paths(pidfile, animator);

    t0 = now_ms();
    BA_CHECK(boot_splash_stop() == true);
    BA_CHECK_MSG(now_ms() - t0 < 50, "sin PIDFILE no tiene que esperar (%llu ms)",
                 (unsigned long long) (now_ms() - t0));
}

static void test_corrupt_pidfile(void) {
    static const char *corruptos[] = { "", "abc", "0", "1", "-5", "12abc", "  ", "99999999999999999999",
                                       "2147483648" };
    size_t i;

    boot_splash_set_paths(pidfile, animator);

    for (i = 0; i < sizeof(corruptos) / sizeof(corruptos[0]); i++) {
        uint64_t t0 = now_ms();

        pidfile_write(corruptos[i]);
        BA_CHECK_MSG(boot_splash_stop() == true, "corrupto \"%s\"", corruptos[i]);
        BA_CHECK_MSG(now_ms() - t0 < 50, "corrupto \"%s\": espero %llu ms", corruptos[i],
                     (unsigned long long) (now_ms() - t0));
    }

    /* El PID de un proceso VIVO pero con un sufijo corrupto no se senaliza. */
    {
        pid_t pid = spawn_animator(NULL);
        char  texto[64];

        (void) snprintf(texto, sizeof(texto), "%ldx", (long) pid);
        pidfile_write(texto);

        BA_CHECK(boot_splash_stop() == true);
        BA_CHECK_MSG(child_alive(pid) == 1, "se mato un proceso con PIDFILE corrupto");

        kill_and_reap(pid);
    }

    pidfile_remove();
}

static void test_foreign_pid(void) {
    /* PID vivo de OTRO ejecutable: la identidad no coincide, no se senaliza. */
    pid_t pid = spawn_animator(NULL);

    boot_splash_set_paths(pidfile, "/usr/sbin/x6200_bootanim"); /* no es el fake_animator */

    {
        char texto[32];

        (void) snprintf(texto, sizeof(texto), "%ld\n", (long) pid);
        pidfile_write(texto);
    }

    BA_CHECK(boot_splash_stop() == true);
    BA_CHECK_MSG(child_alive(pid) == 1, "se senalizo un PID ajeno");

    kill_and_reap(pid);
    pidfile_remove();

    boot_splash_set_paths(pidfile, animator);
}

static void test_dead_process(void) {
    /* Proceso que ya termino (zombi sin recolectar): no hay nada que detener. */
    pid_t    pid = spawn_animator(NULL);
    char     texto[32];
    uint64_t t0;

    kill_and_reap(pid); /* termina y se recolecta: el PID ya no existe */

    (void) snprintf(texto, sizeof(texto), "%ld\n", (long) pid);
    pidfile_write(texto);

    t0 = now_ms();
    BA_CHECK(boot_splash_stop() == true);
    BA_CHECK_MSG(now_ms() - t0 < 50, "con un PID muerto no tiene que esperar");

    /* Y con el proceso todavia en estado zombi (sin recolectar). */
    {
        pid_t  z  = spawn_animator(NULL);
        int    st = 0;

        (void) kill(z, SIGKILL);
        (void) waitpid(z, &st, WNOHANG); /* puede quedar zombi: un zombi no dibuja */

        (void) snprintf(texto, sizeof(texto), "%ld\n", (long) z);
        pidfile_write(texto);

        t0 = now_ms();
        BA_CHECK(boot_splash_stop() == true);
        BA_CHECK_MSG(now_ms() - t0 < 50, "un zombi no tiene que hacer esperar");

        (void) waitpid(z, &st, 0);
    }

    pidfile_remove();
}

static void test_usr1_exit(void) {
    /* Lo normal en la radio: el animador sale con SIGUSR1 y la cesion es
     * inmediata (no espera los 100 ms completos). */
    pid_t    pid = spawn_animator(NULL);
    char     texto[32];
    uint64_t t0, dt;
    int      status;

    (void) snprintf(texto, sizeof(texto), "%ld\n", (long) pid);
    pidfile_write(texto);
    boot_splash_set_paths(pidfile, animator);

    t0 = now_ms();
    BA_CHECK(boot_splash_stop() == true);
    dt = now_ms() - t0;

    BA_CHECK_MSG(dt < BOOT_SPLASH_USR1_WAIT_MS + 60u, "la cesion tardo %llu ms",
                 (unsigned long long) dt);

    status = reap_status(pid);
    BA_CHECK_MSG(WIFSIGNALED(status) && WTERMSIG(status) == SIGUSR1, "salio con %d", status);

    pidfile_remove();
}

static void test_term_fallback(void) {
    /* Ignora SIGUSR1: se agota la espera de 100 ms y sale con SIGTERM. */
    pid_t    pid = spawn_animator("--ignore-usr1");
    char     texto[32];
    uint64_t t0, dt;
    int      status;

    (void) snprintf(texto, sizeof(texto), "%ld\n", (long) pid);
    pidfile_write(texto);
    boot_splash_set_paths(pidfile, animator);

    t0 = now_ms();
    BA_CHECK(boot_splash_stop() == true);
    dt = now_ms() - t0;

    BA_CHECK_MSG(dt >= BOOT_SPLASH_USR1_WAIT_MS, "no espero los 100 ms de SIGUSR1 (%llu ms)",
                 (unsigned long long) dt);
    BA_CHECK_MSG(dt < BOOT_SPLASH_USR1_WAIT_MS + BOOT_SPLASH_TERM_WAIT_MS + 150u,
                 "el handoff tardo de mas (%llu ms)", (unsigned long long) dt);

    status = reap_status(pid);
    BA_CHECK_MSG(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM, "salio con %d", status);

    pidfile_remove();
}

static void test_kill_fallback(void) {
    /* Ignora SIGUSR1 y SIGTERM: ultimo recurso SIGKILL, sin bloquear 100 ms mas. */
    /* Ignora SIGUSR1 y SIGTERM: tienen que agotarse las dos ventanas y quedar
     * solo el SIGKILL. */
    pid_t    pid = spawn_animator2("--ignore-usr1", "--ignore-term");
    char     texto[32];
    uint64_t t0, dt;
    int      status;

    (void) snprintf(texto, sizeof(texto), "%ld\n", (long) pid);
    pidfile_write(texto);
    boot_splash_set_paths(pidfile, animator);

    t0 = now_ms();
    BA_CHECK(boot_splash_stop() == true);
    dt = now_ms() - t0;

    BA_CHECK_MSG(dt >= BOOT_SPLASH_USR1_WAIT_MS + BOOT_SPLASH_TERM_WAIT_MS,
                 "no espero las dos ventanas (%llu ms)", (unsigned long long) dt);
    BA_CHECK_MSG(dt < BOOT_SPLASH_USR1_WAIT_MS + BOOT_SPLASH_TERM_WAIT_MS + BOOT_SPLASH_KILL_WAIT_MS + 200u,
                 "el ultimo recurso bloqueo de mas (%llu ms)", (unsigned long long) dt);

    status = reap_status(pid);
    BA_CHECK_MSG(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL, "salio con %d", status);

    pidfile_remove();
}

int main(int argc, char **argv) {
    char resolved[4096];

    if (argc < 2 || realpath(argv[1], resolved) == NULL) {
        printf("SKIP: hace falta el path del fake_animator (uso: handoff_test <fake_animator>)\n");
        return 77;
    }

    (void) snprintf(animator, sizeof(animator), "%s", resolved);

    /* PIDFILE en un temporal propio del test. */
    (void) snprintf(tmpdir, sizeof(tmpdir), "/tmp/x6200-bootanim-test");
    (void) mkdir(tmpdir, 0700);
    (void) snprintf(pidfile, sizeof(pidfile), "%s/x6200_bootanim.pid", tmpdir);
    pidfile_remove();

    test_no_pidfile();
    test_corrupt_pidfile();
    test_foreign_pid();
    test_dead_process();
    test_usr1_exit();
    test_term_fallback();
    test_kill_fallback();

    BA_TEST_SUMMARY("handoff_test");
}

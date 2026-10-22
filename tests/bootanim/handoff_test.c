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

/* La cesion se juzga por los sondeos instrumentados de boot_splash.c, que solo
 * existen bajo -DBOOTANIM_TEST (macro que unicamente define el gate del host);
 * sin la macro este test no compila, y un error claro es mejor que una cascada
 * de declaraciones implicitas. */
#ifndef BOOTANIM_TEST
#error "handoff_test.c necesita -DBOOTANIM_TEST (instrumentacion de sondeos de boot_splash.c)"
#endif

#include "boot_splash.h"
#include "test_util.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static char animator[4096]; /* path del fake_animator, resuelto */
static char pidfile[128];
static char tmpdir[64];

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

/* Los asserts de "no tiene que esperar" miran los sondeos de la espera
 * instrumentalizada (boot_splash_test_*), nunca el tiempo de pared: un runner
 * de CI cargado puede tardar >50 ms en cualquier trabajo sin que el codigo
 * espere nada. El contrato es "no espera", no "tarda poco". */
static void waits_reset(void) {
    boot_splash_test_reset_waits();
}

static void check_no_waits(const char *caso) {
    BA_CHECK_MSG(boot_splash_test_waits() == 0 && boot_splash_test_waited_ms() == 0,
                 "%s: no tiene que esperar (sondeos=%u, %u ms)", caso, boot_splash_test_waits(),
                 boot_splash_test_waited_ms());
}

/* Presupuesto total del contrato: boot_splash_stop() nunca espera mas que esto. */
#define BS_TOTAL_WAIT_MS (BOOT_SPLASH_USR1_WAIT_MS + BOOT_SPLASH_TERM_WAIT_MS + BOOT_SPLASH_KILL_WAIT_MS)

/* La unica salida false del contrato es DESPUES de agotar el presupuesto
 * (USR1 + TERM + KILL). Bajo un runner muy cargado el proceso puede no morir
 * dentro de las ventanas y el false es legitimo (no un bug): el test NO puede
 * depender de la velocidad con que el sistema operativo deja morir al hijo. */
static void check_stop_result(bool stopped, const char *caso) {
    BA_CHECK_MSG(stopped == true || boot_splash_test_waited_ms() >= BS_TOTAL_WAIT_MS,
                 "%s: stop() devolvio false sin agotar el presupuesto (%u de %u ms)", caso,
                 boot_splash_test_waited_ms(), (unsigned int) BS_TOTAL_WAIT_MS);
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

/* Estado del proceso leido de /proc/<pid>/stat (0 = no se pudo leer: ya no
 * existe o el formato no se entiende; el comm puede tener ')' adentro, asi que
 * se busca el ULTIMO ')'). */
static int read_proc_state(pid_t pid, char *state) {
    char  path[64];
    char  buf[512];
    FILE *f;
    const char *p;

    (void) snprintf(path, sizeof(path), "/proc/%ld/stat", (long) pid);
    f = fopen(path, "r");

    if (f == NULL) {
        return 0;
    }

    if (fgets(buf, sizeof(buf), f) == NULL) {
        (void) fclose(f);
        return 0;
    }

    (void) fclose(f);
    p = strrchr(buf, ')');

    if (p == NULL || p[1] != ' ' || p[2] == '\0') {
        return 0;
    }

    *state = p[2];

    return 1;
}

/* Hasta-condicion con techo (5 s): el hijo quedo en estado zombi ('Z'). El
 * setup NO puede asumir que el SIGKILL ya se materializo: bajo carga el proceso
 * tarda en morir y, si se mide antes, el caso deja de probar un zombi (y el
 * codigo, con razon, espera al proceso vivo). */
static int wait_zombie(pid_t pid) {
    int i;

    for (i = 0; i < 2500; i++) {
        char state = '\0';

        if (!read_proc_state(pid, &state)) {
            return 0; /* ya no existe: no llego a ser zombi */
        }

        if (state == 'Z') {
            return 1;
        }

        {
            struct timespec ts = { 0, 2000000 }; /* 2 ms */

            (void) nanosleep(&ts, NULL);
        }
    }

    return 0;
}

/* Hasta-condicion con techo (5 s): el hijo ya tiene instalado el handler de
 * `signo` (bit signo-1 de SigCgt en /proc/<pid>/status). El setup NO puede
 * asumir que fake_animator ya ignora la senal: si se le manda SIGUSR1 antes,
 * muere con SIGUSR1 y los casos de fallback (SIGTERM/SIGKILL) no prueban nada. */
static int wait_handler(pid_t pid, int signo) {
    char path[64];
    int  i;

    (void) snprintf(path, sizeof(path), "/proc/%ld/status", (long) pid);

    for (i = 0; i < 2500; i++) {
        char               line[256];
        FILE              *f    = fopen(path, "r");
        unsigned long long mask = 0;
        int                found = 0;

        if (f == NULL) {
            return 0;
        }

        while (fgets(line, sizeof(line), f) != NULL) {
            if (strncmp(line, "SigCgt:", 7) == 0) {
                mask  = strtoull(line + 7, NULL, 16);
                found = 1;
                break;
            }
        }

        (void) fclose(f);

        if (found && (mask & (1ULL << (unsigned int) (signo - 1))) != 0ULL) {
            return 1;
        }

        {
            struct timespec ts = { 0, 2000000 }; /* 2 ms */

            (void) nanosleep(&ts, NULL);
        }
    }

    return 0;
}

/* --- Casos ------------------------------------------------------------------ */

static void test_no_pidfile(void) {
    pidfile_remove();
    boot_splash_set_paths(pidfile, animator);

    waits_reset();
    BA_CHECK(boot_splash_stop() == true);
    check_no_waits("sin PIDFILE");
}

static void test_corrupt_pidfile(void) {
    static const char *corruptos[] = { "", "abc", "0", "1", "-5", "12abc", "  ", "99999999999999999999",
                                       "2147483648" };
    size_t i;

    boot_splash_set_paths(pidfile, animator);

    for (i = 0; i < sizeof(corruptos) / sizeof(corruptos[0]); i++) {
        pidfile_write(corruptos[i]);
        waits_reset();
        BA_CHECK_MSG(boot_splash_stop() == true, "corrupto \"%s\"", corruptos[i]);
        check_no_waits(corruptos[i]);
    }

    /* El PID de un proceso VIVO pero con un sufijo corrupto no se senaliza. */
    {
        pid_t pid = spawn_animator(NULL);
        char  texto[64];

        (void) snprintf(texto, sizeof(texto), "%ldx", (long) pid);
        pidfile_write(texto);

        waits_reset();
        BA_CHECK(boot_splash_stop() == true);
        BA_CHECK_MSG(child_alive(pid) == 1, "se mato un proceso con PIDFILE corrupto");
        check_no_waits("PID vivo con sufijo corrupto");

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

    waits_reset();
    BA_CHECK(boot_splash_stop() == true);
    BA_CHECK_MSG(child_alive(pid) == 1, "se senalizo un PID ajeno");

    check_no_waits("PID ajeno (identidad distinta)");

    kill_and_reap(pid);
    pidfile_remove();

    boot_splash_set_paths(pidfile, animator);
}

static void test_dead_process(void) {
    /* Proceso que ya termino (zombi sin recolectar): no hay nada que detener. */
    pid_t pid = spawn_animator(NULL);
    char  texto[32];

    kill_and_reap(pid); /* termina y se recolecta: el PID ya no existe */

    (void) snprintf(texto, sizeof(texto), "%ld\n", (long) pid);
    pidfile_write(texto);

    waits_reset();
    BA_CHECK(boot_splash_stop() == true);
    check_no_waits("PID muerto");

    /* Y con el proceso todavia en estado zombi (sin recolectar). El setup
     * espera (con techo) a que el SIGKILL se materialice en 'Z': sin eso, bajo
     * carga se mediria un proceso todavia vivo y el caso no probaria al zombi. */
    {
        pid_t z  = spawn_animator(NULL);
        int   st = 0;

        (void) kill(z, SIGKILL);
        BA_CHECK_MSG(wait_zombie(z) == 1, "el SIGKILL no dejo al proceso en estado zombi");

        (void) snprintf(texto, sizeof(texto), "%ld\n", (long) z);
        pidfile_write(texto);

        waits_reset();
        BA_CHECK(boot_splash_stop() == true);
        check_no_waits("zombi");

        (void) waitpid(z, &st, 0);
    }

    pidfile_remove();
}

static void test_usr1_exit(void) {
    /* Lo normal en la radio: el animador sale con SIGUSR1 y la cesion es
     * inmediata. El oraculo es la senal de salida del hijo (SIGUSR1, que solo
     * puede venir de que el codigo se lo mando): si el runner esta tan cargado
     * que el hijo tarda en morir, el codigo puede escalar y eso no es un bug. */
    pid_t pid = spawn_animator(NULL);
    char  texto[32];
    int   status;

    (void) snprintf(texto, sizeof(texto), "%ld\n", (long) pid);
    pidfile_write(texto);
    boot_splash_set_paths(pidfile, animator);

    waits_reset();
    check_stop_result(boot_splash_stop(), "cesion normal (SIGUSR1)");

    BA_CHECK_MSG(boot_splash_test_waited_ms() <= BS_TOTAL_WAIT_MS,
                 "la cesion espero mas alla del presupuesto (%u ms)", boot_splash_test_waited_ms());

    status = reap_status(pid);
    BA_CHECK_MSG(WIFSIGNALED(status) && WTERMSIG(status) == SIGUSR1, "salio con %d", status);

    pidfile_remove();
}

static void test_term_fallback(void) {
    /* Ignora SIGUSR1: se agota la espera de 100 ms y sale con SIGTERM. Se mide
     * en sondeos (no en pared): la ventana de SIGUSR1 se consume completa y el
     * total nunca pasa del presupuesto del contrato. */
    pid_t pid = spawn_animator("--ignore-usr1");
    char  texto[32];
    int   status;

    (void) snprintf(texto, sizeof(texto), "%ld\n", (long) pid);
    pidfile_write(texto);
    boot_splash_set_paths(pidfile, animator);

    /* El setup espera a que el hijo tenga el handler puesto: si se le manda
     * SIGUSR1 antes, muere con SIGUSR1 y el caso no prueba el fallback. */
    BA_CHECK_MSG(wait_handler(pid, SIGUSR1) == 1, "el animador no llego a ignorar SIGUSR1");

    waits_reset();
    check_stop_result(boot_splash_stop(), "fallback a SIGTERM");

    BA_CHECK_MSG(boot_splash_test_waited_ms() >= BOOT_SPLASH_USR1_WAIT_MS,
                 "no espero los 100 ms de SIGUSR1 (%u ms)", boot_splash_test_waited_ms());
    BA_CHECK_MSG(boot_splash_test_waited_ms() <= BS_TOTAL_WAIT_MS,
                 "el handoff espero mas alla del presupuesto (%u ms)", boot_splash_test_waited_ms());

    status = reap_status(pid);
    BA_CHECK_MSG(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM, "salio con %d", status);

    pidfile_remove();
}

static void test_kill_fallback(void) {
    /* Ignora SIGUSR1 y SIGTERM: tienen que agotarse las dos ventanas y quedar
     * solo el SIGKILL. Se mide en sondeos: las dos ventanas completas, la de
     * SIGKILL sin pasarse. */
    pid_t pid = spawn_animator2("--ignore-usr1", "--ignore-term");
    char  texto[32];
    int   status;

    (void) snprintf(texto, sizeof(texto), "%ld\n", (long) pid);
    pidfile_write(texto);
    boot_splash_set_paths(pidfile, animator);

    /* Los dos handlers tienen que estar puestos antes de la cesion. */
    BA_CHECK_MSG(wait_handler(pid, SIGUSR1) == 1, "el animador no llego a ignorar SIGUSR1");
    BA_CHECK_MSG(wait_handler(pid, SIGTERM) == 1, "el animador no llego a ignorar SIGTERM");

    waits_reset();
    check_stop_result(boot_splash_stop(), "ultimo recurso SIGKILL");

    BA_CHECK_MSG(boot_splash_test_waited_ms() >= BOOT_SPLASH_USR1_WAIT_MS + BOOT_SPLASH_TERM_WAIT_MS,
                 "no espero las dos ventanas (%u ms)", boot_splash_test_waited_ms());
    BA_CHECK_MSG(boot_splash_test_waited_ms() <= BS_TOTAL_WAIT_MS,
                 "el ultimo recurso espero mas alla del presupuesto (%u ms)", boot_splash_test_waited_ms());

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

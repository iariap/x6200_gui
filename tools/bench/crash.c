/*
 * Captura la senal y deja la pila de llamadas en /crash.txt, sin depender de gdb.
 * Se precarga con LD_PRELOAD junto con la biblioteca de trazas.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define _GNU_SOURCE
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

static int fd = -1;

static void w(const char *s) {
    if (fd < 0) fd = (int) syscall(SYS_open, "/crash.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) syscall(SYS_write, fd, s, strlen(s));
}

static void handler(int sig, siginfo_t *si, void *uc) {
    void  *frames[32];
    char   buf[256];
    int    n;
    char   maps[1024];
    int    m;

    snprintf(buf, sizeof(buf), "\n=== senal %d (%s) ===\n", sig, strsignal(sig));
    w(buf);

    /* El PC del fallo: el binario no es PIE (base 0x400000), asi que la direccion se
     * traduce directo con addr2line. */
    ucontext_t *c = (ucontext_t *) uc;
#if defined(__arm__)
    {
        unsigned long pc   = (unsigned long) c->uc_mcontext.arm_pc;
        unsigned long base = 0;
        FILE         *m    = fopen("/proc/self/maps", "r");
        char          linea[256];

        /* El ejecutable es PIE: la primera línea de maps da su base de carga, así que el
         * offset del fallo dentro del archivo es PC - base y eso lo traduce addr2line. */
        if (m != NULL) {
            if (fgets(linea, sizeof(linea), m) != NULL) {
                sscanf(linea, "%lx-", &base);
            }
            fclose(m);
        }

        snprintf(buf, sizeof(buf), "PC: %p   base: %p   offset para addr2line: 0x%lx   fallo en: %p\n",
                 (void *) pc, (void *) base, base ? pc - base : pc, si ? si->si_addr : NULL);
        w(buf);

        /* Los registros del bucle de copia: dicen qué puntero era el malo. */
        snprintf(buf, sizeof(buf), "r0=%08lx r1=%08lx r2=%08lx r3=%08lx r4=%08lx r5=%08lx r6=%08lx r7=%08lx\n",
                 (unsigned long) c->uc_mcontext.arm_r0, (unsigned long) c->uc_mcontext.arm_r1,
                 (unsigned long) c->uc_mcontext.arm_r2, (unsigned long) c->uc_mcontext.arm_r3,
                 (unsigned long) c->uc_mcontext.arm_r4, (unsigned long) c->uc_mcontext.arm_r5,
                 (unsigned long) c->uc_mcontext.arm_r6, (unsigned long) c->uc_mcontext.arm_r7);
        w(buf);

        /* En ARM el link register guarda la dirección de quien llamó: si el fallo es un salto
         * a NULL (PC = 0), lr es el sitio de la llamada y eso es lo que hay que traducir. */
        {
            unsigned long lr = (unsigned long) c->uc_mcontext.arm_lr;
            unsigned long sp = (unsigned long) c->uc_mcontext.arm_sp;

            snprintf(buf, sizeof(buf), "LR: %p   (llamador: offset 0x%lx)   SP: %p\n",
                     (void *) lr, base ? lr - base : lr, (void *) sp);
            w(buf);
        }
    }
#else
    snprintf(buf, sizeof(buf), "direccion que fallo: %p\n", si ? si->si_addr : NULL);
#endif
    w(buf);

    /* la linea del ejecutable en /proc/self/maps sirve para calcular el offset real */
    m = (int) syscall(SYS_open, "/proc/self/maps", O_RDONLY);
    if (m >= 0) {
        int r = (int) syscall(SYS_read, m, maps, sizeof(maps) - 1);
        if (r > 0) {
            maps[r] = '\0';
            char *nl = strchr(maps, '\n');
            if (nl) *(nl + 1) = '\0';
            snprintf(buf, sizeof(buf), "maps: %s", maps);
            w(buf);
        }
        syscall(SYS_close, m);
    }

    n = backtrace(frames, 32);
    for (int i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "  #%02d %p\n", i, frames[i]);
        w(buf);
    }
    w("=== fin ===\n");
    signal(sig, SIG_DFL);
    raise(sig);
}

__attribute__((constructor)) static void init(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    sa.sa_flags     = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
}

/*
 * Trazas para diagnosticar el arranque de la GUI, sin recompilarla.
 *
 *   LD_PRELOAD=/lib/libx6200trace.so /usr/sbin/x6200_gui
 *
 * Hace dos cosas:
 *   1. deja stdout y stderr sin buffer, así el log de la GUI llega línea a línea en vez
 *      de quedar retenido en el buffer (que es lo que se pierde cuando se cuelga);
 *   2. anota en /boot_trace.txt cada apertura de /dev o /mnt y cada pthread_create, con
 *      tiempo monótono. La última anotación antes del cuelgue es dónde se traba.
 *
 * Las anotaciones usan open/write/close del sistema directamente para no recursar en
 * las funciones que estamos interceptando.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define TRACE "/boot_trace.txt"

static int tr_fd = -1;

static void tr_puts(const char *s) {
    if (tr_fd < 0) {
        tr_fd = (int) syscall(SYS_open, TRACE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    }
    if (tr_fd >= 0) {
        syscall(SYS_write, tr_fd, s, strlen(s));
    }
}

static void mark(const char *fmt, ...) {
    char    out[256];
    int     n;
    va_list ap;
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    n = snprintf(out, sizeof(out), "[%ld.%03ld] ", (long) ts.tv_sec, ts.tv_nsec / 1000000L);
    va_start(ap, fmt);
    n += vsnprintf(out + n, sizeof(out) - n, fmt, ap);
    va_end(ap);
    if (n > (int) sizeof(out) - 2) {
        n = sizeof(out) - 2;
    }
    out[n++] = '\n';
    out[n]   = '\0';
    tr_puts(out);
}

__attribute__((constructor)) static void trace_init(void) {
    char buf[64];

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    snprintf(buf, sizeof(buf), "[trace] libx6200trace pid %d\n", (int) getpid());
    tr_puts(buf);
}

int open(const char *path, int flags, ...) {
    static int (*real)(const char *, int, ...);
    mode_t mode = 0;

    if (!real) {
        real = dlsym(RTLD_NEXT, "open");
    }
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    if (path && (!strncmp(path, "/dev/", 5) || !strncmp(path, "/mnt/", 5))) {
        mark("open(%s)", path);
    }
    return real(path, flags, mode);
}

int open64(const char *path, int flags, ...) {
    static int (*real)(const char *, int, ...);
    mode_t mode = 0;

    if (!real) {
        real = dlsym(RTLD_NEXT, "open64");
    }
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    if (path && (!strncmp(path, "/dev/", 5) || !strncmp(path, "/mnt/", 5))) {
        mark("open64(%s)", path);
    }
    return real(path, flags, mode);
}

FILE *fopen(const char *path, const char *mode) {
    static FILE *(*real)(const char *, const char *);

    if (!real) {
        real = dlsym(RTLD_NEXT, "fopen");
    }
    if (path) {
        mark("fopen(%s)", path);
    }
    return real(path, mode);
}

FILE *fopen64(const char *path, const char *mode) {
    static FILE *(*real)(const char *, const char *);

    if (!real) {
        real = dlsym(RTLD_NEXT, "fopen64");
    }
    if (path) {
        mark("fopen64(%s)", path);
    }
    return real(path, mode);
}

int pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*fn)(void *), void *arg) {
    static int (*real)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);

    if (!real) {
        real = dlsym(RTLD_NEXT, "pthread_create");
    }
    mark("pthread_create(%p)", (void *) fn);
    return real(t, attr, fn, arg);
}

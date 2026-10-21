/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Shim de test (LD_PRELOAD) para correr el x6200_bootanim REAL en el host sin
 *  /dev/fb0: intercepta open() para redirigir /dev/fb0 a un archivo del banco,
 *  ioctl() para devolver fb_fix_screeninfo/fb_var_screeninfo sinteticas (config
 *  por variables de entorno) y, opcionalmente, el reloj monotono para correr la
 *  animacion acelerada y probar de verdad el corte de los 30 s.
 *
 *  NO es parte de la ROM: vive en tests/bootanim y solo lo carga el gate.
 *
 *  Variables de entorno:
 *    BA_TEST_FB           archivo que hace de /dev/fb0 (sin esto, /dev/fb0 real)
 *    BA_TEST_BPP          16 o 32 (default 16)
 *    BA_TEST_XRES/YRES    resolucion visible (default 480x800)
 *    BA_TEST_XOFF/YOFF    offsets (default 0)
 *    BA_TEST_PAD          bytes extra por linea (puede ser negativo; default 0)
 *    BA_TEST_SMEM_EXTRA   bytes de mas declarados en smem_len (rompe el mmap)
 *    BA_TEST_SMEM_SHORT   bytes de menos en smem_len (geometria insuficiente)
 *    BA_TEST_BITFIELDS    rgb (default) | bgr
 *    BA_TEST_SPEED        multiplicador del reloj monotono (default 1 = real)
 */
#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int (*real_open)(const char *, int, ...);
static int (*real_open64)(const char *, int, ...);
static int (*real_openat)(int, const char *, int, ...);
static int (*real_ioctl)(int, unsigned long, ...);
static int (*real_clock_gettime)(clockid_t, struct timespec *);
static int (*real_clock_nanosleep)(clockid_t, int, const struct timespec *, struct timespec *);

static int    fake_fd = -1;
static size_t fake_len = 0;

static char  target[4096];
static int   target_ready = 0;
static int   cfg_bpp = 16;
static int   cfg_xres = 480;
static int   cfg_yres = 800;
static long  cfg_xoff = 0;
static long  cfg_yoff = 0;
static long  cfg_pad = 0;
static long  cfg_smem_extra = 0;
static long  cfg_smem_short = 0;
static int   cfg_bgr = 0;
static double cfg_speed = 1.0;
static int   cfg_ready = 0;

static struct timespec v_start;
static struct timespec r_start;

__attribute__((constructor)) static void shim_init(void) {
    const char *env;

    real_open = dlsym(RTLD_NEXT, "open");
    real_open64 = dlsym(RTLD_NEXT, "open64");
    real_openat = dlsym(RTLD_NEXT, "openat");
    real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    real_clock_gettime = dlsym(RTLD_NEXT, "clock_gettime");
    real_clock_nanosleep = dlsym(RTLD_NEXT, "clock_nanosleep");

    env = getenv("BA_TEST_FB");

    if (env != NULL && env[0] != '\0') {
        (void) snprintf(target, sizeof(target), "%s", env);
        target_ready = 1;
    }
}

static void cfg_load(void) {
    const char *env;

    if (cfg_ready) {
        return;
    }

    cfg_ready = 1;

    if ((env = getenv("BA_TEST_BPP")) != NULL) {
        cfg_bpp = atoi(env);
    }

    if ((env = getenv("BA_TEST_XRES")) != NULL) {
        cfg_xres = atoi(env);
    }

    if ((env = getenv("BA_TEST_YRES")) != NULL) {
        cfg_yres = atoi(env);
    }

    if ((env = getenv("BA_TEST_XOFF")) != NULL) {
        cfg_xoff = atol(env);
    }

    if ((env = getenv("BA_TEST_YOFF")) != NULL) {
        cfg_yoff = atol(env);
    }

    if ((env = getenv("BA_TEST_PAD")) != NULL) {
        cfg_pad = atol(env);
    }

    if ((env = getenv("BA_TEST_SMEM_EXTRA")) != NULL) {
        cfg_smem_extra = atol(env);
    }

    if ((env = getenv("BA_TEST_SMEM_SHORT")) != NULL) {
        cfg_smem_short = atol(env);
    }

    if ((env = getenv("BA_TEST_BITFIELDS")) != NULL && strcmp(env, "bgr") == 0) {
        cfg_bgr = 1;
    }

    if ((env = getenv("BA_TEST_SPEED")) != NULL) {
        cfg_speed = atof(env);

        if (cfg_speed <= 0.0) {
            cfg_speed = 1.0;
        }
    }

    (void) real_clock_gettime(CLOCK_MONOTONIC, &r_start);
    v_start = r_start;
}

static int is_fb_path(const char *path) {
    if (path == NULL) {
        return 0;
    }

    return strcmp(path, "/dev/fb0") == 0;
}

static int shim_open(const char *path, int flags, ...) {
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list ap;

        va_start(ap, flags);
        mode = (mode_t) va_arg(ap, int);
        va_end(ap);
    }

    if (real_open == NULL) {
        real_open = dlsym(RTLD_NEXT, "open");
    }

    cfg_load();

    if (target_ready && is_fb_path(path)) {
        fake_fd = real_open(target, flags, mode);
        return fake_fd;
    }

    return real_open(path, flags, mode);
}

int open(const char *path, int flags, ...) {
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list ap;

        va_start(ap, flags);
        mode = (mode_t) va_arg(ap, int);
        va_end(ap);
    }

    return shim_open(path, flags, mode);
}

int open64(const char *path, int flags, ...) {
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list ap;

        va_start(ap, flags);
        mode = (mode_t) va_arg(ap, int);
        va_end(ap);
    }

    return shim_open(path, flags, mode);
}

int openat(int dirfd, const char *path, int flags, ...) {
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list ap;

        va_start(ap, flags);
        mode = (mode_t) va_arg(ap, int);
        va_end(ap);
    }

    if (real_openat == NULL) {
        real_openat = dlsym(RTLD_NEXT, "openat");
    }

    cfg_load();

    if (target_ready && is_fb_path(path)) {
        fake_fd = real_openat(dirfd, target, flags, mode);
        return fake_fd;
    }

    return real_openat(dirfd, path, flags, mode);
}

static unsigned int fb_line_length(void) {
    long line = (long) cfg_xres * (cfg_bpp / 8) + cfg_pad;

    if (line < 0) {
        line = 0;
    }

    return (unsigned int) line;
}

static unsigned int fb_smem_len(void) {
    long len = (long) fb_line_length() * cfg_yres + cfg_smem_extra - cfg_smem_short;

    if (len < 0) {
        len = 0;
    }

    return (unsigned int) len;
}

static void fill_fields(struct fb_var_screeninfo *var) {
    if (cfg_bpp == 32) {
        if (cfg_bgr) {
            var->red.offset   = 0;
            var->green.offset = 8;
            var->blue.offset  = 16;
        } else {
            var->red.offset   = 16;
            var->green.offset = 8;
            var->blue.offset  = 0;
        }

        var->red.length   = 8;
        var->green.length = 8;
        var->blue.length  = 8;
    } else {
        var->red.offset   = 11;
        var->green.offset = 5;
        var->blue.offset  = 0;
        var->red.length   = 5;
        var->green.length = 6;
        var->blue.length  = 5;
    }
}

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    void   *arg;

    va_start(ap, request);
    arg = va_arg(ap, void *);
    va_end(ap);

    if (real_ioctl == NULL) {
        real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    }

    cfg_load();

    if (fd == fake_fd && fake_fd >= 0) {
        if (request == FBIOGET_FSCREENINFO) {
            struct fb_fix_screeninfo *fix = arg;

            memset(fix, 0, sizeof(*fix));
            (void) snprintf(fix->id, sizeof(fix->id), "%s", "bootanim-fake");
            fix->smem_start  = 0;
            fix->smem_len    = fb_smem_len();
            fix->type        = FB_TYPE_PACKED_PIXELS;
            fix->visual      = FB_VISUAL_TRUECOLOR;
            fix->line_length = fb_line_length();

            return 0;
        }

        if (request == FBIOGET_VSCREENINFO) {
            struct fb_var_screeninfo *var = arg;

            memset(var, 0, sizeof(*var));
            var->xres           = (unsigned int) cfg_xres;
            var->yres           = (unsigned int) cfg_yres;
            var->xres_virtual   = (unsigned int) cfg_xres;
            var->yres_virtual   = (unsigned int) cfg_yres;
            var->xoffset        = (unsigned int) cfg_xoff;
            var->yoffset        = (unsigned int) cfg_yoff;
            var->bits_per_pixel = (unsigned int) cfg_bpp;
            fill_fields(var);

            return 0;
        }
    }

    return real_ioctl(fd, request, arg);
}

/* mmap del archivo que hace de framebuffer: se deja pasar al mmap real, pero se
   recuerda el largo pedido (para que el banco sepa que se mapeo todo). */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

static void *(*real_mmap)(void *, size_t, int, int, int, off_t);

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if (real_mmap == NULL) {
        real_mmap = dlsym(RTLD_NEXT, "mmap");
    }

    if (fd == fake_fd && fake_fd >= 0) {
        fake_len = length;

        /* El banko puede pedir que el mmap falle: es el unico camino de la
         * geometria/memoria que no se puede provocar con un archivo normal
         * (mapear mas alla del EOF no falla en Linux). */
        if (getenv("BA_TEST_MMAP_FAIL") != NULL) {
            errno = ENOMEM;
            return MAP_FAILED;
        }
    }

    return real_mmap(addr, length, prot, flags, fd, offset);
}

static struct timespec virt_from_real(const struct timespec *real_now) {
    struct timespec out;
    double          dt;

    dt = (double) (real_now->tv_sec - r_start.tv_sec) +
         (double) (real_now->tv_nsec - r_start.tv_nsec) / 1e9;
    dt *= cfg_speed;

    out.tv_sec  = v_start.tv_sec + (time_t) dt;
    out.tv_nsec = v_start.tv_nsec + (long) ((dt - (double) (time_t) dt) * 1e9);

    while (out.tv_nsec >= 1000000000L) {
        out.tv_nsec -= 1000000000L;
        out.tv_sec += 1;
    }

    return out;
}

static struct timespec real_from_virt(const struct timespec *virt) {
    struct timespec out;
    double          dt;

    dt = ((double) (virt->tv_sec - v_start.tv_sec) +
          (double) (virt->tv_nsec - v_start.tv_nsec) / 1e9) /
         cfg_speed;

    out.tv_sec  = r_start.tv_sec + (time_t) dt;
    out.tv_nsec = r_start.tv_nsec + (long) ((dt - (double) (time_t) dt) * 1e9);

    while (out.tv_nsec >= 1000000000L) {
        out.tv_nsec -= 1000000000L;
        out.tv_sec += 1;
    }

    while (out.tv_nsec < 0) {
        out.tv_nsec += 1000000000L;
        out.tv_sec -= 1;
    }

    return out;
}

int clock_gettime(clockid_t clockid, struct timespec *tp) {
    if (real_clock_gettime == NULL) {
        real_clock_gettime = dlsym(RTLD_NEXT, "clock_gettime");
    }

    cfg_load();

    if (clockid != CLOCK_MONOTONIC || cfg_speed == 1.0) {
        return real_clock_gettime(clockid, tp);
    }

    {
        struct timespec real_now;
        int             rc = real_clock_gettime(CLOCK_MONOTONIC, &real_now);

        if (rc != 0) {
            return rc;
        }

        *tp = virt_from_real(&real_now);
    }

    return 0;
}

int clock_nanosleep(clockid_t clockid, int flags, const struct timespec *request, struct timespec *remain) {
    if (real_clock_nanosleep == NULL) {
        real_clock_nanosleep = dlsym(RTLD_NEXT, "clock_nanosleep");
    }

    cfg_load();

    if (clockid != CLOCK_MONOTONIC || cfg_speed == 1.0) {
        return real_clock_nanosleep(clockid, flags, request, remain);
    }

    if (flags & TIMER_ABSTIME) {
        struct timespec real_target = real_from_virt(request);

        return real_clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &real_target, remain);
    }

    {
        struct timespec real_req;
        double          dt = (double) request->tv_sec + (double) request->tv_nsec / 1e9;

        dt /= cfg_speed;

        real_req.tv_sec  = (time_t) dt;
        real_req.tv_nsec = (long) ((dt - (double) (time_t) dt) * 1e9);

        return real_clock_nanosleep(CLOCK_MONOTONIC, 0, &real_req, remain);
    }
}

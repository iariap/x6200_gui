/*
 * Framebuffer falso para poder correr la GUI en un chroot, sin radio.
 *
 *   LD_PRELOAD=/lib/libx6200trace.so:/lib/libfakefb.so /usr/sbin/x6200_gui
 *
 * Intercepta /dev/fb0: las aperturas van a un archivo común y los ioctl de
 * framebuffer (FBIOGET_FSCREENINFO, FBIOGET_VSCREENINFO, FBIOPAN_DISPLAY, FBIOBLANK)
 * devuelven una pantalla de 480x800 a 32 bits, que es la del equipo. El mmap del
 * archivo funciona normal, así que la GUI dibuja sobre él sin enterarse.
 *
 * También deja pasar /dev/input/eventN: si el archivo no existe, lo redirige a
 * /dev/null para que la GUI no muera al abrirlos (no habrá eventos, que es lo que
 * queremos).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define FB_PATH     "/dev/fb0"
#define FB_BACKING  "/tmp/fakefb0"

static int es_fb(const char *p)    { return p && !strcmp(p, FB_PATH); }
static int es_entrada(const char *p) { return p && !strncmp(p, "/dev/input/", 11); }

static int abrir_falso(const char *path, int flags, mode_t mode) {
    static int (*real)(const char *, int, ...);

    if (!real) {
        real = dlsym(RTLD_NEXT, "open");
    }
    if (es_fb(path)) {
        /* un archivo de 8 MB hace de pantalla */
        int fd = real(FB_BACKING, O_RDWR | O_CREAT, 0644);
        if (fd >= 0) {
            ftruncate(fd, 8 * 1024 * 1024);
        }
        return fd;
    }
    if (es_entrada(path) && access(path, F_OK) != 0) {
        return real("/dev/null", flags, mode);
    }
    return real(path, flags, mode);
}

int open(const char *path, int flags, ...) {
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return abrir_falso(path, flags, mode);
}

int open64(const char *path, int flags, ...) {
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return abrir_falso(path, flags, mode);
}

int ioctl(int fd, unsigned long req, ...) {
    static int (*real)(int, unsigned long, ...);
    void *arg;

    if (!real) {
        real = dlsym(RTLD_NEXT, "ioctl");
    }

    va_list ap;
    va_start(ap, req);
    arg = va_arg(ap, void *);
    va_end(ap);

    switch (req) {
        case FBIOGET_FSCREENINFO: {
            struct fb_fix_screeninfo *f = arg;

            memset(f, 0, sizeof(*f));
            snprintf(f->id, sizeof(f->id), "fakefb");
            f->smem_start  = 0;
            f->smem_len    = 8 * 1024 * 1024;
            f->type        = FB_TYPE_PACKED_PIXELS;
            f->visual      = FB_VISUAL_TRUECOLOR;
            f->line_length = 480 * 4;
            return 0;
        }
        case FBIOGET_VSCREENINFO: {
            struct fb_var_screeninfo *v = arg;

            memset(v, 0, sizeof(*v));
            v->xres           = 480;
            v->yres           = 800;
            v->xres_virtual   = 480;
            v->yres_virtual   = 800;
            v->bits_per_pixel = 32;
            v->red.offset   = 16; v->red.length   = 8;
            v->green.offset = 8;  v->green.length = 8;
            v->blue.offset  = 0;  v->blue.length  = 8;
            v->transp.offset = 24; v->transp.length = 8;
            v->activate = FB_ACTIVATE_NOW;
            return 0;
        }
        case FBIOPAN_DISPLAY:
        case FBIOBLANK:
        case FBIOGETCMAP:
        case FBIOPUTCMAP:
        case FBIO_WAITFORVSYNC:
            return 0;
        default:
            break;
    }

    return real(fd, req, arg);
}

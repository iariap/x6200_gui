/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Tabla de modulos cargados para el CRASHLOG (issue #131).
 *
 *  El handler de crashes corre en contexto de senal: no puede alocar ni leer
 *  archivos con comodidad. Por eso todo el trabajo caro (leer /proc/self/maps y
 *  calcular el md5 de cada .so) se hace una sola vez al arrancar y el bloque de
 *  lineas queda en .bss. El handler solo hace write(2) de ese bloque.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "crashlog_modules.h"
#include <link.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------ MD5 ---
 * Implementacion chica y determinista (RFC 1321). No usa heap: el md5 de los
 * modulos se calcula con un buffer fijo en la pila.
 */

typedef struct {
    uint32_t state[4];
    uint64_t bits;
    uint8_t  buffer[64];
} md5_ctx_t;

static uint32_t rotl32(uint32_t x, unsigned n) {
    return (x << n) | (x >> (32 - n));
}

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };
    static const uint8_t s[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
    };
    uint32_t m[16];
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];

    for (int i = 0; i < 16; i++) {
        m[i] = (uint32_t) block[i * 4] |
               ((uint32_t) block[i * 4 + 1] << 8) |
               ((uint32_t) block[i * 4 + 2] << 16) |
               ((uint32_t) block[i * 4 + 3] << 24);
    }

    for (int i = 0; i < 64; i++) {
        uint32_t f;
        int      g;

        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) & 15;
        }

        uint32_t tmp = d;
        d = c;
        c = b;
        b = b + rotl32(a + f + k[i] + m[g], s[i]);
        a = tmp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void md5_init(md5_ctx_t *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->bits     = 0;
}

static void md5_update(md5_ctx_t *ctx, const void *data, size_t len) {
    const uint8_t *p    = (const uint8_t *) data;
    size_t         used = (size_t) ((ctx->bits >> 3) & 63);

    ctx->bits += (uint64_t) len << 3;

    if (used) {
        size_t need = 64 - used;

        if (len < need) {
            memcpy(ctx->buffer + used, p, len);
            return;
        }
        memcpy(ctx->buffer + used, p, need);
        md5_transform(ctx->state, ctx->buffer);
        p   += need;
        len -= need;
    }

    while (len >= 64) {
        md5_transform(ctx->state, p);
        p   += 64;
        len -= 64;
    }

    if (len) {
        memcpy(ctx->buffer, p, len);
    }
}

static void md5_final(md5_ctx_t *ctx, char out[CRASHLOG_MD5_HEX_LEN + 1]) {
    static const char hex[] = "0123456789abcdef";
    uint8_t           pad[72];
    uint64_t          bits = ctx->bits;
    size_t            used = (size_t) ((bits >> 3) & 63);
    size_t            pad_len = (used < 56) ? (56 - used) : (120 - used);

    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    for (int i = 0; i < 8; i++) {
        pad[pad_len + i] = (uint8_t) (bits >> (8 * i));
    }
    md5_update(ctx, pad, pad_len + 8);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            uint8_t byte = (uint8_t) (ctx->state[i] >> (8 * j));
            out[(i * 4 + j) * 2]     = hex[byte >> 4];
            out[(i * 4 + j) * 2 + 1] = hex[byte & 15];
        }
    }
    out[CRASHLOG_MD5_HEX_LEN] = '\0';
}

void crashlog_md5_hex(const void *data, size_t len, char out[CRASHLOG_MD5_HEX_LEN + 1]) {
    md5_ctx_t ctx;

    md5_init(&ctx);
    md5_update(&ctx, data, len);
    md5_final(&ctx, out);
}

int crashlog_md5_file(const char *path, char out[CRASHLOG_MD5_HEX_LEN + 1]) {
    md5_ctx_t ctx;
    uint8_t   buf[8192];
    int       fd = open(path, O_RDONLY);

    if (fd < 0) {
        memset(out, '0', CRASHLOG_MD5_HEX_LEN);
        out[CRASHLOG_MD5_HEX_LEN] = '\0';
        return -1;
    }

    md5_init(&ctx);

    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            memset(out, '0', CRASHLOG_MD5_HEX_LEN);
            out[CRASHLOG_MD5_HEX_LEN] = '\0';
            return -1;
        }
        if (n == 0) {
            break;
        }
        md5_update(&ctx, buf, (size_t) n);
    }

    close(fd);
    md5_final(&ctx, out);
    return 0;
}

/* ------------------------------------------------------------- tabla --- */

static void path_store(char *dst, const char *src) {
    size_t len = strlen(src);

    if (len < CRASHLOG_MOD_PATH_MAX) {
        memcpy(dst, src, len + 1);
        return;
    }

    /* Se conserva la cola (nombre del .so) y se marca el corte con '~': mas
     * util que quedarse con el prefijo /usr/lib/... y perder el archivo. */
    size_t tail = CRASHLOG_MOD_PATH_MAX - 2;        /* '~' + cola + NUL */

    dst[0] = '~';
    memcpy(dst + 1, src + len - tail, tail);
    dst[tail + 1] = '\0';
}

void crashlog_mod_table_reset(crashlog_mod_table_t *table) {
    if (table == NULL) {
        return;
    }

    memset(table, 0, sizeof(*table));
}

bool crashlog_mod_table_add(crashlog_mod_table_t *table, uintptr_t base,
                            const char *md5, const char *path) {
    if (table == NULL || path == NULL || path[0] == '\0') {
        return false;
    }

    if (table->count >= CRASHLOG_MOD_MAX) {
        table->omitted++;
        return false;
    }

    crashlog_mod_t *mod = &table->mods[table->count++];

    mod->base = base;
    memset(mod->md5, 0, sizeof(mod->md5));
    if (md5 != NULL) {
        strncpy(mod->md5, md5, CRASHLOG_MD5_HEX_LEN);
    }
    path_store(mod->path, path);

    return true;
}

size_t crashlog_mod_table_add_from_maps(crashlog_mod_table_t *table, const char *maps_path) {
    FILE *f;
    char  line[512];
    size_t added = 0;

    if (table == NULL || maps_path == NULL) {
        return 0;
    }

    f = fopen(maps_path, "r");
    if (f == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        unsigned long start, end, offset;
        char          perms[8];
        int           path_pos = 0;

        if (sscanf(line, "%lx-%lx %7s %lx %*s %*s %n",
                   &start, &end, perms, &offset, &path_pos) < 4) {
            continue;
        }

        /* Solo mappings ejecutables de archivos absolutos: el ejecutable y las
         * .so. [vdso]/[heap]/[stack] no tienen archivo que hashear. */
        if (perms[2] != 'x' || offset > start) {
            continue;
        }

        char *path = line + path_pos;
        while (*path == ' ' || *path == '\t') {
            path++;
        }

        size_t path_len = strlen(path);
        while (path_len > 0 && (path[path_len - 1] == '\n' || path[path_len - 1] == '\r')) {
            path[--path_len] = '\0';
        }

        if (path[0] != '/') {
            continue;
        }

        bool seen = false;
        for (size_t i = 0; i < table->count; i++) {
            if (strcmp(table->mods[i].path, path) == 0) {
                seen = true;
                break;
            }
        }
        if (seen) {
            continue;
        }

        char md5[CRASHLOG_MD5_HEX_LEN + 1];
        crashlog_md5_file(path, md5);

        if (crashlog_mod_table_add(table, (uintptr_t) (start - offset), md5, path)) {
            added++;
        }
    }

    fclose(f);
    return added;
}

size_t crashlog_mod_render(const crashlog_mod_table_t *table, char *out, size_t out_size) {
    size_t needed = 0;

    if (out != NULL && out_size > 0) {
        out[0] = '\0';
    }

    if (table == NULL) {
        return 0;
    }

    for (size_t i = 0; i < table->count; i++) {
        const crashlog_mod_t *mod = &table->mods[i];
        int n = snprintf(NULL, 0, "CRASHLOG-MOD %08lx %s %s\n",
                         (unsigned long) mod->base, mod->md5, mod->path);

        if (n < 0) {
            return 0;
        }
        needed += (size_t) n;
    }

    if (out == NULL || out_size == 0) {
        return needed;
    }
    if (needed + 1 > out_size) {
        return needed;
    }

    size_t pos = 0;
    for (size_t i = 0; i < table->count; i++) {
        const crashlog_mod_t *mod = &table->mods[i];
        int n = snprintf(out + pos, out_size - pos, "CRASHLOG-MOD %08lx %s %s\n",
                         (unsigned long) mod->base, mod->md5, mod->path);
        pos += (size_t) n;
    }

    return pos;
}

/* ---------------------------------------------------------- arranque --- */

static crashlog_mod_table_t crashlog_table;
static char                 crashlog_block[CRASHLOG_MOD_BLOCK_MAX];
static size_t               crashlog_block_len;

typedef struct {
    crashlog_mod_table_t *table;
    const char           *exe_path;
} phdr_ctx_t;

static int phdr_cb(struct dl_phdr_info *info, size_t size, void *data) {
    (void) size;

    phdr_ctx_t *ctx  = (phdr_ctx_t *) data;
    const char *path = info->dlpi_name;

    if (path == NULL || path[0] == '\0') {
        path = ctx->exe_path;       /* el ejecutable principal no tiene nombre */
    }
    if (path == NULL || path[0] != '/') {
        return 0;                   /* [vdso], linux-vdso.so.1, etc. */
    }

    for (size_t i = 0; i < ctx->table->count; i++) {
        if (strcmp(ctx->table->mods[i].path, path) == 0) {
            return 0;
        }
    }

    char md5[CRASHLOG_MD5_HEX_LEN + 1];
    crashlog_md5_file(path, md5);
    crashlog_mod_table_add(ctx->table, (uintptr_t) info->dlpi_addr, md5, path);

    return 0;
}

void crashlog_modules_init(const char *exe_path) {
    crashlog_mod_table_reset(&crashlog_table);
    crashlog_mod_table_add_from_maps(&crashlog_table, "/proc/self/maps");

    if (crashlog_table.count == 0) {
        phdr_ctx_t ctx = { .table = &crashlog_table, .exe_path = exe_path };
        dl_iterate_phdr(phdr_cb, &ctx);
    }

    crashlog_block_len = crashlog_mod_render(&crashlog_table, crashlog_block,
                                             sizeof(crashlog_block));
}

const char *crashlog_modules_block(size_t *len) {
    if (len != NULL) {
        *len = crashlog_block_len;
    }

    return crashlog_block_len > 0 ? crashlog_block : NULL;
}

size_t crashlog_modules_count(void) {
    return crashlog_table.count;
}

const crashlog_mod_t *crashlog_modules_first(void) {
    return crashlog_table.count > 0 ? &crashlog_table.mods[0] : NULL;
}

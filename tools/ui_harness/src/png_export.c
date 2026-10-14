/*
 *  PNG export for the host UI harness (see png_export.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"
#include "png_export.h"

bool png_export(const char *path)
{
    const lv_color_t *fb = display_framebuffer();
    uint32_t          w  = display_width();
    uint32_t          h  = display_height();

    FILE *f = fopen(path, "wb");

    if (!f) {
        fprintf(stderr, "png_export: cannot open %s\n", path);
        return false;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   info = png_create_info_struct(png);

    if (!png || !info) {
        fprintf(stderr, "png_export: libpng init failed\n");
        fclose(f);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "png_export: libpng write error\n");
        png_destroy_write_struct(&png, &info);
        fclose(f);
        return false;
    }

    png_init_io(png, f);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    uint8_t *row = malloc(w * 3);

    if (!row) {
        fprintf(stderr, "png_export: out of memory\n");
        png_destroy_write_struct(&png, &info);
        fclose(f);
        return false;
    }

    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            const lv_color_t c = fb[y * w + x];

            row[x * 3 + 0] = c.ch.red;
            row[x * 3 + 1] = c.ch.green;
            row[x * 3 + 2] = c.ch.blue;
        }
        png_write_row(png, row);
    }

    free(row);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(f);
    return true;
}

uint64_t png_framebuffer_hash(void)
{
    const uint8_t *p = (const uint8_t *) display_framebuffer();
    size_t         n = (size_t) display_width() * display_height() * sizeof(lv_color_t);
    uint64_t       h = 1469598103934665603ull;      /* FNV-1a 64 */

    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

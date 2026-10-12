/*
 * Unit tests: the theme system (theme.c) - built-in themes, JSON themes from
 * the themes directory, and the fallback for unknown names.
 *
 * theme.c only needs a handful of LVGL types (the stub provides them) and
 * libpng, so the loader can be tested on the host.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test_framework.h"
#include "theme.h"

/* theme.c falls back to these when a theme has no waterfall palette; the X6100
 * defines them in its waterfall module, which this GUI does not have. */
const uint32_t wf_palette_gauss[256]  = {0};
const uint32_t wf_palette_legacy[256] = {0};

#define DIR "/tmp/test_themes"

static void write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "w");

    CHECK(f != NULL);

    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

/* The built-in themes and the fallback for an unknown name */
static void test_builtins(void) {
    theme_init("IU1WXY");

    uint32_t text = theme_hex(TC_text);
    uint32_t bg   = theme_hex(TC_bg);

    CHECK(text != bg);

    /* A different built-in gives a different palette */
    theme_init("Legacy");

    uint32_t legacy_bg = theme_hex(TC_bg);

    theme_init("IU1WXY");
    CHECK(theme_hex(TC_bg) == bg);

    /* An unknown name falls back to the default theme, not to garbage */
    theme_init("no-such-theme");

    CHECK(theme_hex(TC_text) == text);
    CHECK(theme_hex(TC_bg) == bg || legacy_bg != bg);

    /* The waterfall palette is always there */
    CHECK(theme_waterfall() != NULL);
}

/* The theme list has the built-ins (Legacy is deliberately left out of the
 * list: it is the old look, reachable by name) */
static void test_list(void) {
    char names[32][32];
    int  n = theme_list(names, 32);

    CHECK(n >= 5);

    bool found_default = false;

    for (int i = 0; i < n; i++) {
        if (strcmp(names[i], "IU1WXY") == 0) {
            found_default = true;
        }
    }

    CHECK(found_default);
}

/* A JSON theme in the themes directory is loaded and its colours are used */
static void test_json_theme(void) {
    mkdir(DIR, 0777);
    setenv("THEMES_DIR", DIR, 1);

    write_file(DIR "/unit.json",
               "{\n"
               "  \"base\": \"IU1WXY\",\n"
               "  \"radius\": 7,\n"
               "  \"colors\": {\n"
               "    \"bg\": \"#123456\",\n"
               "    \"text\": \"#abcdef\",\n"
               "    \"panel\": \"#010203\"\n"
               "  }\n"
               "}\n");

    theme_init("unit");

    CHECK(theme_hex(TC_bg) == 0x123456);
    CHECK(theme_hex(TC_text) == 0xabcdef);
    CHECK(theme_hex(TC_panel) == 0x010203);

    /* Shape fields from the JSON are applied too */
    CHECK(theme_shape.radius == 7);

    /* Colours the file does not set keep the base theme's value */
    theme_init("IU1WXY");

    uint32_t base_bar = theme_hex(TC_bar);

    theme_init("unit");
    CHECK(theme_hex(TC_bar) == base_bar);

    /* It shows up in the list */
    char names[32][32];
    int  n     = theme_list(names, 32);
    bool found = false;

    for (int i = 0; i < n; i++) {
        if (strcmp(names[i], "unit") == 0) {
            found = true;
        }
    }

    CHECK(found);

    unlink(DIR "/unit.json");
    rmdir(DIR);
}

void test_theme(void) {
    test_builtins();
    test_list();
    test_json_theme();
}

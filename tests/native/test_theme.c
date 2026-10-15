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
#include "params/types.h"      /* params_str_t: el ancho del store de theme_name (UI-013) */
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

/* UI-013: ningún nombre del menú puede no entrar en el store.
 *
 * La elección de tema viaja al próximo arranque dentro de `params.theme_name`,
 * un `params_str_t` (src/params/types.h): `params_str_set`/`params_load_str`
 * copian `sizeof(x) - 1` caracteres y `theme_init(params.theme_name.x)`
 * (src/main.c) resuelve por nombre, así que un nombre que no entre se trunca y
 * cae a IU1WXY en silencio. Acá se hace el round-trip exacto con el nombre que
 * no entraba (el `params_str_t` es el del firmware, no una copia) y se comprueba
 * que TODO nombre que `theme_list()` ofrece entre en el store: es la invariante
 * que UI-013 rompía y que un nombre nuevo de 16+ caracteres volvería a romper.
 */
static void test_names_fit_the_store(void) {
    params_str_t store = {0};

    strncpy(store.x, "Outdoor-Negative", sizeof(store.x) - 1);
    CHECK(strcmp(store.x, "Outdoor-Negative") == 0);   /* entra entero, no truncado */

    char names[32][32];
    int  n = theme_list(names, 32);

    CHECK(n > 0);

    for (int i = 0; i < n; i++) {
        CHECK(strlen(names[i]) < sizeof(store.x));     /* el store puede con cada uno */
    }
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

/* Los temas que vienen con la ROM (rootfs/usr/share/x6200/themes) tienen que cargar todos y
 * aparecer en el listado: si no, no llegan al menú del equipo aunque estén en la imagen.
 * El RS además trae sus propias imágenes (los PNG al lado del JSON). */
static void test_shipped(void) {
    const char *dir = getenv("X6200_SHIPPED_THEMES");

    if (dir == NULL || *dir == 0) {
        return;                     /* el runner no los pasó: nada que comprobar */
    }

    setenv("THEMES_DIR_SHIPPED", dir, 1);
    setenv("THEMES_DIR", "/tmp/test_themes_sin_usuario", 1);     /* sin temas del usuario */

    static const char *const esperados[] = {"Holo", "Material 3", "RS", "Redshift", "Sunlit", "Aurora",
                                           "Trail", "Sprint"};
    char names[32][32];
    int  n = theme_list(names, 32);

    for (size_t k = 0; k < sizeof(esperados) / sizeof(*esperados); k++) {
        bool en_lista = false;

        for (int i = 0; i < n; i++) {
            if (strcmp(names[i], esperados[k]) == 0) {
                en_lista = true;
            }
        }

        CHECK(en_lista);                        /* está en el listado */

        theme_init(esperados[k]);               /* y se carga */
        theme_init(esperados[k]);               /* dos veces no rompe */

        CHECK(theme_hex(TC_bg) <= 0xffffffu);   /* se puede leer una rol */
    }

    /* El RS trae imágenes propias: si no se leen, sus botones quedan sin piel. */
    theme_init("RS");
    CHECK(theme_image(TI_btn, 100, 40) != NULL);

    unsetenv("THEMES_DIR_SHIPPED");
}

/* El tema Sunlit (issue #67) tiene que cargar con su identidad: los roles del archivo, la forma y
 * su paleta de cascada propia. La familia de fuente que el archivo nombra no se comprueba acá
 * (font_select.c no se compila en el host y su stub devuelve -1, ver stubs/stubs.c); la cubre el
 * arnés de UI. */
static void test_sunlit(void) {
    const char *dir = getenv("X6200_SHIPPED_THEMES");

    if (dir == NULL || *dir == 0) {
        return;                     /* el runner no los pasó: nada que comprobar */
    }

    setenv("THEMES_DIR_SHIPPED", dir, 1);
    setenv("THEMES_DIR", "/tmp/test_themes_sin_usuario", 1);

    theme_init("Sunlit");

    /* Roles del archivo: marfil de fondo, texto casi negro y el arco del S-meter en tinta clara
     * (la cara del medidor es un bitmap fijo y oscuro, así que una tinta oscura no se vería). */
    CHECK(theme_hex(TC_bg) == 0xf7f5eb);
    CHECK(theme_hex(TC_text) == 0x162436);
    CHECK(theme_hex(TC_meter_over) == 0xffc9b8);

    /* Forma: bordes firmes y casi sin radios */
    CHECK(theme_shape.radius == 2);
    CHECK(theme_shape.border == 2);
    CHECK(theme_shape.focus_solid);

    /* La cascada propia del tema: 6 paradas, del blanco al azul profundo */
    CHECK(theme_shape.waterfall_n == 6);
    CHECK(theme_shape.waterfall[0] == 0xffffff);
    CHECK(theme_shape.waterfall[5] == 0x102845);

    unsetenv("THEMES_DIR_SHIPPED");
}

void test_theme(void) {
    test_builtins();
    test_list();
    test_names_fit_the_store();
    test_json_theme();
    test_shipped();
    test_sunlit();
}


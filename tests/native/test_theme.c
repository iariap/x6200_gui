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

/* The default theme (issue #74): an empty, unknown or retired name loads Aurora,
 * never a built-in. Aurora is the ROM file rootfs/usr/share/x6200/themes/Aurora.json,
 * so the runner has to pass X6200_SHIPPED_THEMES for the shipped part. */
static void test_default(void) {
    const char *dir = getenv("X6200_SHIPPED_THEMES");

    if (dir == NULL || *dir == 0) {
        return;                     /* el runner no los pasó: nada que comprobar */
    }

    setenv("THEMES_DIR_SHIPPED", dir, 1);
    setenv("THEMES_DIR", "/tmp/test_themes_sin_usuario", 1);     /* sin temas del usuario */

    /* Los roles de Aurora (el archivo), el default de la colección. */
    theme_init("Aurora");
    uint32_t aurora_bg   = theme_hex(TC_bg);
    uint32_t aurora_text = theme_hex(TC_text);

    CHECK(aurora_bg == 0x1a1227);
    CHECK(aurora_text == 0xf8eeff);
    CHECK(strcmp(theme_shape.name, "Aurora") == 0);

    /* Un nombre vacío, uno desconocido y uno retirado (built-ins legados, archivos
     * Holo/Material 3/RS) caen TODOS al default: Aurora, no IU1WXY ni ningún built-in. */
    static const char *const retirados[] = {"", "no-such-theme", "IU1WXY", "Classic", "Outdoor",
                                            "Outdoor-Negative", "Legacy", "VISION", "Holo", "Material 3", "RS"};

    for (size_t i = 0; i < sizeof(retirados) / sizeof(retirados[0]); i++) {
        theme_init(retirados[i]);
        CHECK(theme_hex(TC_bg) == aurora_bg);
        CHECK(theme_hex(TC_text) == aurora_text);
        CHECK(strcmp(theme_shape.name, "Aurora") == 0);
    }

    /* NULL también: no hay nombre que cargar. */
    theme_init(NULL);
    CHECK(theme_hex(TC_bg) == aurora_bg);

    /* La paleta del waterfall siempre está. */
    CHECK(theme_waterfall() != NULL);

    unsetenv("THEMES_DIR_SHIPPED");
}

/* El catálogo de Ajustes ofrece SOLO los seis temas de la colección (docs/THEMES.md):
 * ni un built-in legado ni un archivo retirado tiene entrada en la lista. */
static void test_list(void) {
    static const char *const seis[] = {"Sunlit", "Redshift", "Sprint", "Cathode", "Trail", "Aurora"};
    const int               n_seis = (int)(sizeof(seis) / sizeof(seis[0]));
    char                    names[32][32];
    int                     n = theme_list(names, 32);

    CHECK(n == n_seis);

    for (int i = 0; i < n; i++) {
        CHECK(i < n_seis && strcmp(names[i], seis[i]) == 0);
    }

    /* Ningún retirado entra en la lista ni se ofrece. */
    for (int i = 0; i < n; i++) {
        CHECK(strcmp(names[i], "IU1WXY") != 0);
        CHECK(strcmp(names[i], "Classic") != 0);
        CHECK(strcmp(names[i], "Outdoor") != 0);
        CHECK(strcmp(names[i], "Holo") != 0);
        CHECK(strcmp(names[i], "RS") != 0);
    }

    for (size_t k = 0; k < sizeof(seis) / sizeof(seis[0]); k++) {
        CHECK(theme_offered(seis[k]));           /* los seis sí */
    }
    CHECK(!theme_offered("IU1WXY"));
    CHECK(!theme_offered("Outdoor"));
    CHECK(!theme_offered("Holo"));
    CHECK(!theme_offered(""));
    CHECK(!theme_offered(NULL));
}

/* UI-013: ningún nombre del menú puede no entrar en el store.
 *
 * La elección de tema viaja al próximo arranque dentro de `params.theme_name`,
 * un `params_str_t` (src/params/types.h): `params_str_set`/`params_load_str`
 * copian `sizeof(x) - 1` caracteres y `theme_init(params.theme_name.x)`
 * (src/main.c) resuelve por nombre, así que un nombre que no entre se trunca y
 * cae al default. El nombre que rompía UI-013 (Outdoor-Negative, 16) se retiró en
 * #74; el store se queda ancho (32) y acá se comprueba que TODO nombre que
 * `theme_list()` ofrece entre entero.
 */
static void test_names_fit_the_store(void) {
    params_str_t store = {0};

    /* El store todavía aguanta un nombre de 16: no es la causa del truncado. */
    strncpy(store.x, "Outdoor-Negative", sizeof(store.x) - 1);
    CHECK(strcmp(store.x, "Outdoor-Negative") == 0);   /* entra entero, no truncado */

    char names[32][32];
    int  n = theme_list(names, 32);

    CHECK(n > 0);

    for (int i = 0; i < n; i++) {
        CHECK(strlen(names[i]) < sizeof(store.x));     /* el store puede con cada uno */
    }
}

/* A JSON theme in the themes directory is loaded and its colours are used. It inherits the
 * `base` template (IU1WXY is still a base, not a theme, issue #74). */
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

    /* Colours the file does not set keep the base template's value (IU1WXY, la
     * columna de la tabla compilada). */
    theme_init("unit");

    uint32_t base_bar = theme_hex(TC_bar);

    CHECK(base_bar == 0x3a3f47);     /* IU1WXY bar (src/theme.h) */

    /* Un tema del usuario (DATA) no entra en el menú: el selector sólo lista los seis */
    char names[32][32];
    int  n     = theme_list(names, 32);
    bool found = false;

    for (int i = 0; i < n; i++) {
        if (strcmp(names[i], "unit") == 0) {
            found = true;
        }
    }

    CHECK(!found);

    unlink(DIR "/unit.json");
    rmdir(DIR);
}

/* Los seis temas que vienen con la ROM (rootfs/usr/share/x6200/themes) son la oferta del
 * selector y todos cargan. Los archivos legados (Holo, Material 3, RS) ya no están: su nombre
 * cae al default (Aurora), no carga un tema. */
static void test_shipped(void) {
    const char *dir = getenv("X6200_SHIPPED_THEMES");

    if (dir == NULL || *dir == 0) {
        return;                     /* el runner no los pasó: nada que comprobar */
    }

    setenv("THEMES_DIR_SHIPPED", dir, 1);
    setenv("THEMES_DIR", "/tmp/test_themes_sin_usuario", 1);     /* sin temas del usuario */

    static const char *const esperados[] = {"Sunlit", "Redshift", "Sprint", "Cathode", "Trail", "Aurora"};
    const int               n_esp        = (int)(sizeof(esperados) / sizeof(esperados[0]));
    char                    names[32][32];
    int                     n = theme_list(names, 32);

    CHECK(n == n_esp);                              /* exactamente los seis, en orden */

    for (int k = 0; k < n_esp; k++) {
        bool en_lista = false;

        for (int i = 0; i < n; i++) {
            if (strcmp(names[i], esperados[k]) == 0) {
                en_lista = true;
            }
        }

        CHECK(en_lista);                        /* está en el listado */
        CHECK(strcmp(names[k], esperados[k]) == 0);

        theme_init(esperados[k]);               /* y se carga */
        theme_init(esperados[k]);               /* dos veces no rompe */

        CHECK(strcmp(theme_shape.name, esperados[k]) == 0);
        CHECK(theme_hex(TC_bg) <= 0xffffffu);   /* se puede leer una rol */
    }

    /* Los archivos legados salieron de la ROM (#74): cargarlos por nombre NO carga un tema,
     * cae al default Aurora. */
    theme_init("Aurora");

    uint32_t aurora_bg = theme_hex(TC_bg);

    static const char *const retirados[] = {"Holo", "Material 3", "RS"};

    for (size_t k = 0; k < sizeof(retirados) / sizeof(*retirados); k++) {
        theme_init(retirados[k]);
        CHECK(strcmp(theme_shape.name, "Aurora") == 0);
        CHECK(theme_hex(TC_bg) == aurora_bg);
    }

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

/* El tema Cathode (issue #70) tiene que cargar con su identidad: los roles del archivo, la forma y
 * su paleta de cascada propia. La familia de fuente que el archivo nombra no se comprueba acá
 * (font_select.c no se compila en el host y su stub devuelve -1, ver stubs/stubs.c); la cubre el
 * arnés de UI. */
static void test_cathode(void) {
    const char *dir = getenv("X6200_SHIPPED_THEMES");

    if (dir == NULL || *dir == 0) {
        return;                     /* el runner no los pasó: nada que comprobar */
    }

    setenv("THEMES_DIR_SHIPPED", dir, 1);
    setenv("THEMES_DIR", "/tmp/test_themes_sin_usuario", 1);

    theme_init("Cathode");

    /* Roles del archivo: fondo café-negro, texto sepia y el arco del S-meter en ámbar claro (la
     * cara del medidor es un bitmap fijo y oscuro, así que una tinta oscura no se vería). */
    CHECK(theme_hex(TC_bg) == 0x100b07);
    CHECK(theme_hex(TC_text) == 0xf9d9aa);
    CHECK(theme_hex(TC_meter_over) == 0xffd6a0);

    /* Forma: cantos casi cuadrados, borde de 1 px y foco sólido */
    CHECK(theme_shape.radius == 1);
    CHECK(theme_shape.radius_btn == 2);
    CHECK(theme_shape.radius_popup == 3);
    CHECK(theme_shape.border == 1);
    CHECK(theme_shape.focus_solid);

    /* La cascada propia del tema: 7 paradas, del negro café a la crema pergamino */
    CHECK(theme_shape.waterfall_n == 7);
    CHECK(theme_shape.waterfall[0] == 0x090605);
    CHECK(theme_shape.waterfall[6] == 0xfff1cb);

    unsetenv("THEMES_DIR_SHIPPED");
}

/* `waterfall_signal_width` (decision del owner, 2026-10-15): el ancho de la linea
 * central del waterfall (la marca de senal sobre la paleta, src/waterfall.c) es una
 * clave de tema en px. Default 1 y clamp 1..8: los temas outdoor la piden en 3 y un
 * tema que no la define queda en 1 px, retrocompatible. */
static void test_waterfall_signal_width(void) {
    const char *dir = getenv("X6200_SHIPPED_THEMES");

    if (dir && *dir) {
        setenv("THEMES_DIR_SHIPPED", dir, 1);
        setenv("THEMES_DIR", "/tmp/test_themes_sin_usuario", 1);
    }

    /* El default del struct (via use_base) y de un tema que no define la clave: 1 px.
     * Un nombre retirado (IU1WXY, Classic) cae al default de la colección (Aurora),
     * que tampoco define la clave, así que también da 1 px. */
    theme_init("IU1WXY");
    CHECK(theme_shape.waterfall_signal_width == 1);

    theme_init("Classic");
    CHECK(theme_shape.waterfall_signal_width == 1);

    if (dir == NULL || *dir == 0) {
        return;                     /* el runner no los pasó: nada que comprobar */
    }

    /* Los dos temas outdoor la traen mas gruesa (3 px). */
    theme_init("Sunlit");
    CHECK(theme_shape.waterfall_signal_width == 3);

    theme_init("Trail");
    CHECK(theme_shape.waterfall_signal_width == 3);

    /* Un tema que no la define queda en 1 px: nada cambia para el resto. */
    theme_init("Redshift");
    CHECK(theme_shape.waterfall_signal_width == 1);

    theme_init("Aurora");
    CHECK(theme_shape.waterfall_signal_width == 1);

    /* El clamp 1..8 del parser: 0 (o negativo) cae a 1, por encima de 8 cae a 8. */
    mkdir(DIR, 0777);
    setenv("THEMES_DIR", DIR, 1);

    write_file(DIR "/width0.json", "{\n  \"waterfall_signal_width\": 0\n}\n");
    write_file(DIR "/widthneg.json", "{\n  \"waterfall_signal_width\": -5\n}\n");
    write_file(DIR "/width9.json", "{\n  \"waterfall_signal_width\": 9\n}\n");
    write_file(DIR "/width3.json", "{\n  \"waterfall_signal_width\": 3\n}\n");

    theme_init("width0");
    CHECK(theme_shape.waterfall_signal_width == 1);

    theme_init("widthneg");
    CHECK(theme_shape.waterfall_signal_width == 1);

    theme_init("width9");
    CHECK(theme_shape.waterfall_signal_width == 8);

    theme_init("width3");
    CHECK(theme_shape.waterfall_signal_width == 3);

    unlink(DIR "/width0.json");
    unlink(DIR "/widthneg.json");
    unlink(DIR "/width9.json");
    unlink(DIR "/width3.json");
    rmdir(DIR);

    unsetenv("THEMES_DIR_SHIPPED");
}

/* issue #91: un tema que no se puede resolver (ni archivo del usuario ni de la coleccion) tiene
 * que caer al default COMPLETO. El default es THEME_DEFAULT (Aurora, issue #74): los 122 roles,
 * el nombre y la forma salen del default, ninguno queda del tema pedido ni a medio aplicar
 * (mitad-y-mitad), y pedirlo dos veces da lo mismo (sin estado que se arrastre entre intentos). */
static void test_unknown_theme_is_complete_default(void) {
    const char *dir = getenv("X6200_SHIPPED_THEMES");
    uint32_t    def[TC_COUNT];
    int16_t     def_radius, def_border;

    if (dir == NULL || *dir == 0) {
        return;                     /* el runner no los pasó: nada que comprobar */
    }

    setenv("THEMES_DIR_SHIPPED", dir, 1);
    setenv("THEMES_DIR", "/tmp/test_themes_sin_usuario", 1);     /* sin temas del usuario */

    /* El default de la coleccion, cargado por su nombre: la referencia. */
    theme_init(THEME_DEFAULT);
    CHECK(strcmp(theme_shape.name, THEME_DEFAULT) == 0);

    for (int i = 0; i < TC_COUNT; i++) def[i] = theme_hex((theme_color_t) i);

    def_radius = theme_shape.radius;
    def_border = theme_shape.border;

    theme_init("no-such-theme-91");

    for (int i = 0; i < TC_COUNT; i++) {
        CHECK(theme_hex((theme_color_t) i) == def[i]);
    }
    CHECK(strcmp(theme_shape.name, THEME_DEFAULT) == 0);        /* el nombre es el del default */
    CHECK(theme_shape.radius == def_radius);                    /* y la forma tambien */
    CHECK(theme_shape.border == def_border);
    CHECK(theme_waterfall() != NULL);

    /* Idempotente: pedirlo de nuevo da exactamente lo mismo */
    theme_init("no-such-theme-91");
    for (int i = 0; i < TC_COUNT; i++) {
        CHECK(theme_hex((theme_color_t) i) == def[i]);
    }
    CHECK(strcmp(theme_shape.name, THEME_DEFAULT) == 0);

    /* Y no mezcla con el tema que hubiera antes: cargar otro tema y despues pedir el
     * inexistente deja el estado identico al default, no una mitad de cada uno. */
    theme_init("Sunlit");
    theme_init("no-such-theme-91");
    for (int i = 0; i < TC_COUNT; i++) {
        CHECK(theme_hex((theme_color_t) i) == def[i]);
    }
    CHECK(strcmp(theme_shape.name, THEME_DEFAULT) == 0);
    CHECK(theme_shape.radius == def_radius);

    unsetenv("THEMES_DIR_SHIPPED");
}

/* Y un archivo de tema que EXISTE pero no se puede leer (JSON roto, DATA a medio copiar) cae
 * igual al default completo: la resolucion no puede depender de que el archivo sea valido. */
static void test_broken_theme_file_falls_back(void) {
    const char *dir = getenv("X6200_SHIPPED_THEMES");
    uint32_t    def[TC_COUNT];

    if (dir == NULL || *dir == 0) {
        return;                     /* el runner no los pasó: nada que comprobar */
    }

    setenv("THEMES_DIR_SHIPPED", dir, 1);
    mkdir(DIR, 0777);
    setenv("THEMES_DIR", DIR, 1);
    write_file(DIR "/roto91.json", "{ esto no es json");

    theme_init(THEME_DEFAULT);
    for (int i = 0; i < TC_COUNT; i++) def[i] = theme_hex((theme_color_t) i);

    theme_init("roto91");       /* el archivo existe, pero no parsea */

    for (int i = 0; i < TC_COUNT; i++) {
        CHECK(theme_hex((theme_color_t) i) == def[i]);
    }
    CHECK(strcmp(theme_shape.name, THEME_DEFAULT) == 0);
    CHECK(theme_waterfall() != NULL);

    unlink(DIR "/roto91.json");
    rmdir(DIR);
    unsetenv("THEMES_DIR_SHIPPED");
}

void test_theme(void) {
    test_default();
    test_list();
    test_names_fit_the_store();
    test_json_theme();
    test_shipped();
    test_sunlit();
    test_cathode();
    test_waterfall_signal_width();
    test_unknown_theme_is_complete_default();
    test_broken_theme_file_falls_back();
}


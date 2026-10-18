/*
 * Native test for the CRASHLOG module table (issue #131).
 *
 * The crash handler cannot resolve a PC that falls in a shared library if the
 * log does not say which libraries and which base addresses were loaded. The
 * fix emits one `CRASHLOG-MOD <base> <md5> <path>` line per module, built once
 * at startup (no heap, so it is safe to use from the signal handler).
 *
 * This test pins:
 *   1. the exact, deterministic line format;
 *   2. that a path longer than the slot is truncated by the tail (keeping the
 *      file name) and never breaks the line;
 *   3. that the renderer reports the needed size and does not write a partial
 *      line when the output buffer is too small;
 *   4. the MD5 implementation against the standard vectors.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

#include "crashlog_modules.h"

#include "test_framework.h"

static void test_format_is_deterministic(void) {
    crashlog_mod_table_t table;
    char                out[1024];
    char                again[1024];

    crashlog_mod_table_reset(&table);
    CHECK(crashlog_mod_table_add(&table, 0x47ea8c,
                                 "b1ba67520079904449b6932a112d3c78",
                                 "/usr/sbin/x6200_gui"));
    CHECK(crashlog_mod_table_add(&table, 0xb6170000,
                                 "cd30ca6c58ad42f06caad930b65f0ee2",
                                 "/usr/lib/liblvgl.so"));
    CHECK(crashlog_mod_table_add(&table, 0xb6100000,
                                 "412df301b11eeecfaaa77847fe63372b",
                                 "/lib/libc.so.6"));

    const char *want =
        "CRASHLOG-MOD 0047ea8c b1ba67520079904449b6932a112d3c78 /usr/sbin/x6200_gui\n"
        "CRASHLOG-MOD b6170000 cd30ca6c58ad42f06caad930b65f0ee2 /usr/lib/liblvgl.so\n"
        "CRASHLOG-MOD b6100000 412df301b11eeecfaaa77847fe63372b /lib/libc.so.6\n";

    size_t n = crashlog_mod_render(&table, out, sizeof(out));
    CHECK(n == strlen(want));
    CHECK(strcmp(out, want) == 0);

    /* Repetir el render no cambia nada: el bloque se arma una vez y se escribe
     * tal cual desde el handler. */
    size_t n2 = crashlog_mod_render(&table, again, sizeof(again));
    CHECK(n2 == n);
    CHECK(strcmp(again, out) == 0);
}

static void test_long_path_is_truncated_by_the_tail(void) {
    crashlog_mod_table_t table;
    char                out[512];
    char                long_path[300];

    memset(long_path, 'x', sizeof(long_path));
    const char *head = "/aaaa/bbbb/cccc/dddd/eeee/ffff/gggg/hhhh/";
    memcpy(long_path, head, strlen(head));
    memcpy(long_path + sizeof(long_path) - 24, "/lib/libnombre-largo.so", 23);
    long_path[sizeof(long_path) - 1] = '\0';

    crashlog_mod_table_reset(&table);
    CHECK(crashlog_mod_table_add(&table, 0xb6000000,
                                 "0123456789abcdef0123456789abcdef", long_path));

    size_t n = crashlog_mod_render(&table, out, sizeof(out));
    CHECK(n > 0);
    CHECK(n < sizeof(out));
    const char *prefix = "CRASHLOG-MOD b6000000 0123456789abcdef0123456789abcdef ~";
    CHECK(strncmp(out, prefix, strlen(prefix)) == 0);
    CHECK(strstr(out, "/lib/libnombre-largo.so") != NULL);
    CHECK(strlen(out) == n);
    CHECK(out[n - 1] == '\n');

    /* Cuatro campos por linea: la ruta truncada no agrega espacios ni saltos. */
    int spaces = 0;
    for (size_t i = 0; i < n - 1; i++) {
        if (out[i] == ' ') spaces++;
    }
    CHECK(spaces == 3);

    /* La ruta guardada nunca llega a CRASHLOG_MOD_PATH_MAX y queda terminada. */
    size_t path_len = strlen(table.mods[0].path);
    CHECK(path_len < CRASHLOG_MOD_PATH_MAX);
    CHECK(table.mods[0].path[0] == '~');
}

static void test_short_buffer_does_not_write_partial_output(void) {
    crashlog_mod_table_t table;
    char                small[16];

    crashlog_mod_table_reset(&table);
    CHECK(crashlog_mod_table_add(&table, 0x47ea8c,
                                 "b1ba67520079904449b6932a112d3c78",
                                 "/usr/sbin/x6200_gui"));

    memset(small, 0xAA, sizeof(small));
    size_t needed = crashlog_mod_render(&table, small, sizeof(small));
    CHECK(needed > sizeof(small));
    CHECK(small[0] == '\0');
}

static void test_empty_table_renders_nothing(void) {
    crashlog_mod_table_t table;
    char                out[64];

    crashlog_mod_table_reset(&table);
    CHECK(table.count == 0);
    CHECK(crashlog_mod_render(&table, out, sizeof(out)) == 0);
    CHECK(out[0] == '\0');
}

static void test_md5_standard_vectors(void) {
    char hex[CRASHLOG_MD5_HEX_LEN + 1];

    crashlog_md5_hex("", 0, hex);
    CHECK(strcmp(hex, "d41d8cd98f00b204e9800998ecf8427e") == 0);

    crashlog_md5_hex("abc", 3, hex);
    CHECK(strcmp(hex, "900150983cd24fb0d6963f7d28e17f72") == 0);

    crashlog_md5_hex("The quick brown fox jumps over the lazy dog", 43, hex);
    CHECK(strcmp(hex, "9e107d9d372bb6826bd81d3542a419d6") == 0);
}

void test_crashlog_modules(void) {
    test_format_is_deterministic();
    test_long_path_is_truncated_by_the_tail();
    test_short_buffer_does_not_write_partial_output();
    test_empty_table_renders_nothing();
    test_md5_standard_vectors();
}

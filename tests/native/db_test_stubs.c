/*
 * Stubs del test de persistencia (issue #213).
 *
 * - `_lv_log_add`: captura los mensajes de LV_LOG_* en un buffer para poder
 *   afirmarlos (y contar repeticiones) sin hardware ni salida de la radio.
 * - `theme_offered`: lo referencia la migracion 3 (issue #74) de
 *   src/params/migrations.c; aca se listan los seis temas que la ROM ofrece,
 *   igual que tests/native/migrations_stubs.c.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "stubs_db/lvgl/src/misc/lv_log.h"
#include "db_test_stubs.h"

static char db_test_log[8192];
static int  db_test_log_lines = 0;

void db_test_log_reset(void) {
    db_test_log[0]   = 0;
    db_test_log_lines = 0;
}

const char *db_test_log_text(void) {
    return db_test_log;
}

int db_test_log_lines_count(void) {
    return db_test_log_lines;
}

int db_test_log_occurrences(const char *needle) {
    int    n    = 0;
    size_t len  = strlen(needle);

    if (len == 0) {
        return 0;
    }
    for (const char *p = db_test_log; (p = strstr(p, needle)) != NULL; p += len) {
        n++;
    }
    return n;
}

void _lv_log_add(lv_log_level_t level, const char *file, int line,
                 const char *func, const char *format, ...) {
    (void) level;
    (void) file;
    (void) line;
    (void) func;

    char    text[1024];
    va_list args;

    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);

    db_test_log_lines++;

    size_t used = strlen(db_test_log);

    if (used + strlen(text) + 2 < sizeof db_test_log) {
        snprintf(db_test_log + used, sizeof db_test_log - used, "%s\n", text);
    }
}

bool theme_offered(const char *name) {
    static const char *offered[] = { "Aurora", "Cathode", "Redshift",
                                     "Sprint", "Sunlit", "Trail" };

    for (size_t i = 0; i < sizeof(offered) / sizeof(offered[0]); i++) {
        if (name != NULL && strcmp(name, offered[i]) == 0) {
            return true;
        }
    }
    return false;
}

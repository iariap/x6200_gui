/*
 * End-to-end de la persistencia fail-closed con las rutas REALES del firmware
 * (/mnt/params.db y /usr/share/x6200/params.default.db), issue #213.
 *
 * Este binario llama al `database_init()` real de src/params/db.c (se linkea el
 * archivo tal cual sale del repo, no se incluye) y lo corre en un chroot privado
 * donde /mnt es de verdad un montaje (tmpfs rw, tmpfs RO, tmpfs llena, o un
 * directorio sin montar). El orquestador es tests/native/run_db_failclosed.sh.
 *
 *   db_end_to_end --make-db <fixture>       crea /mnt/params.db con ese fixture
 *   db_end_to_end --make-template <t>       escribe la plantilla (valid|bad|none)
 *   db_end_to_end <caso>                    corre el caso y afirma el resultado
 *
 * Cada caso deja una linea `case=... init=... result=PASS|FAIL` en la salida.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <errno.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "db_fixtures.h"
#include "db_test_stubs.h"

#include "params/db.h"

#define PARAMS_DB      "/mnt/params.db"
#define TEMPLATE_DB    "/usr/share/x6200/params.default.db"

static int failures = 0;

static void check(bool ok, const char *what) {
    if (!ok) {
        printf("    FALLO: %s\n", what);
        failures++;
    }
}

static bool file_hash(const char *path, uint64_t *hash) {
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        return false;
    }

    uint64_t h    = 1469598103934665603ull;
    int      byte;

    while ((byte = fgetc(f)) != EOF) {
        h ^= (uint64_t) (unsigned char) byte;
        h *= 1099511628211ull;
    }
    fclose(f);
    *hash = h;
    return true;
}

static int query_int(const char *path, const char *sql) {
    sqlite3      *h    = NULL;
    sqlite3_stmt *stmt = NULL;
    int           val  = -9999;

    if (sqlite3_open_v2(path, &h, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        sqlite3_close(h);
        return -9999;
    }
    if (sqlite3_prepare_v2(h, sql, -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            val = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(h);
    return val;
}

static bool db_exists(void) {
    return access(PARAMS_DB, F_OK) == 0;
}

/* --- casos ----------------------------------------------------------------- */

/* Base valida: el arranque tiene que pasar y NO tocar el archivo. */
static void case_valid(void) {
    uint64_t before = 0, after = 0;

    check(file_hash(PARAMS_DB, &before), "hash previo");
    check(database_init(), "database_init() con base valida");
    check(file_hash(PARAMS_DB, &after), "hash posterior");
    check(before == after, "la base valida no se reescribe");
    check(query_int(PARAMS_DB, "SELECT id FROM version") == 5, "version 5");
    check(query_int(PARAMS_DB, "SELECT val FROM band_params WHERE bands_id=1 AND name='vfoa_freq'") == 7074000,
          "preset intacto");
    printf("case=valid init=true bytes_unchanged=%s version=%d\n",
           before == after ? "true" : "false", query_int(PARAMS_DB, "SELECT id FROM version"));
}

/* Base ausente en DATA valida: se siembra la plantilla, byte por byte. */
static void case_absent(void) {
    check(!db_exists(), "la base no existe antes del arranque");
    check(database_init(), "database_init() siembra la plantilla");
    check(db_exists(), "la base quedo creada");

    uint64_t h_db = 0, h_tmpl = 0;

    check(file_hash(PARAMS_DB, &h_db), "hash de la base sembrada");
    check(file_hash(TEMPLATE_DB, &h_tmpl), "hash de la plantilla");
    check(h_db == h_tmpl, "la base sembrada es identica a la plantilla");

    struct stat st = { 0 };

    check(stat(PARAMS_DB, &st) == 0, "stat de la base sembrada");
    check((st.st_mode & 07777) == 0644, "modo 0644");
    check(st.st_size > 0, "base no vacia");
    check(query_int(PARAMS_DB, "SELECT id FROM version") == 5, "version 5");

    char leftovers[64];
    int  n = 0;

    for (int i = 0; i < 32; i++) {
        snprintf(leftovers, sizeof leftovers, "/mnt/params.db.tmp%02d", i);
        if (access(leftovers, F_OK) == 0) {
            n++;
        }
    }
    check(n == 0, "sin temporales de la instalacion");
    printf("case=absent init=true seeded_from_template=%s mode=0%o\n",
           h_db == h_tmpl ? "true" : "false", (unsigned) (st.st_mode & 07777));
}

/* Plantilla ausente: no se inventa una base vacia ni se sigue. */
static void case_absent_no_template(void) {
    check(!db_exists(), "la base no existe antes del arranque");
    check(!database_init(), "database_init() falla sin plantilla");
    check(!db_exists(), "no se creo ninguna base");
    printf("case=absent-no-template init=false created=false\n");
}

/* DATA sin montar: /mnt es un directorio del rootfs. */
static void case_no_mount(void) {
    check(!db_exists(), "la base no existe antes del arranque");
    check(!database_init(), "database_init() falla si DATA no esta montada");
    check(!db_exists(), "no se creo /mnt/params.db sobre el rootfs");
    printf("case=no-mount init=false created=false\n");
}

/* Base corrupta: se aborta y no se toca ni un byte. */
static void case_corrupt(void) {
    uint64_t before = 0, after = 0;

    check(file_hash(PARAMS_DB, &before), "hash previo");
    check(!database_init(), "database_init() falla con base corrupta");
    check(file_hash(PARAMS_DB, &after), "hash posterior");
    check(before == after, "la base corrupta queda intacta");
    printf("case=corrupt init=false bytes_unchanged=%s\n", before == after ? "true" : "false");
}

/* DATA montada en RO: se aborta sin escribir (ni renombrar) la base. */
static void case_ro(void) {
    uint64_t before = 0, after = 0;

    check(file_hash(PARAMS_DB, &before), "hash previo");
    check(!database_init(), "database_init() falla con DATA en RO");
    check(file_hash(PARAMS_DB, &after), "hash posterior");
    check(before == after, "la base queda intacta en RO");
    printf("case=ro init=false bytes_unchanged=%s\n", before == after ? "true" : "false");
}

/* DATA llena: el arranque pasa (la base es valida) pero la escritura tiene que
 * FALLAR y reportarse, sin limpiar el dirty. */
static void case_full(void) {
    check(database_init(), "database_init() con DATA llena");

    db_test_log_reset();

    bool dirty = true;

    params_write_int("probe_full", 1234, &dirty);
    check(dirty, "dirty se conserva si la escritura falla");
    check(db_test_log_occurrences("Persistence write failed") == 1, "el fallo de escritura se reporta");
    check(query_int(PARAMS_DB, "SELECT count(*) FROM params WHERE name='probe_full'") == 0,
          "el valor no quedo persistido");
    printf("case=full init=true write_failed=%s dirty_kept=%s errors=%d\n",
           db_test_log_occurrences("Persistence write failed") == 1 ? "true" : "false",
           dirty ? "true" : "false", db_test_log_occurrences("Persistence write failed"));
}

/* Migracion 2 (ATU) desde una base vieja: se convierte UNA vez. */
static void case_v1_atu(void) {
    check(database_init(), "database_init() migra una base vieja");
    check(query_int(PARAMS_DB, "SELECT id FROM version") == 5, "version migrada a 5");
    check(query_int(PARAMS_DB, "SELECT freq FROM atu WHERE ant=1 AND val=100") == 3 * 50000 + 25000,
          "indice convertido a Hz");
    check(query_int(PARAMS_DB, "SELECT freq FROM atu WHERE ant=1 AND val=200") == 5 * 50000 + 25000,
          "segundo indice convertido a Hz");
    printf("case=v1-atu init=true version=5 freq=%d,%d\n",
           query_int(PARAMS_DB, "SELECT freq FROM atu WHERE ant=1 AND val=100"),
           query_int(PARAMS_DB, "SELECT freq FROM atu WHERE ant=1 AND val=200"));
}

/* ATU con las dos formas: procedencia ambigua -> aborta sin convertir. */
static void case_v1_atu_mixed(void) {
    uint64_t before = 0, after = 0;

    check(file_hash(PARAMS_DB, &before), "hash previo");
    check(!database_init(), "database_init() aborta con ATU ambigua");
    check(file_hash(PARAMS_DB, &after), "hash posterior");
    check(before == after, "la base ambigua queda intacta");
    check(query_int(PARAMS_DB, "SELECT id FROM version") == 1, "la version no sube");
    check(query_int(PARAMS_DB, "SELECT freq FROM atu WHERE ant=1 AND val=100") == 3, "no se convirtio nada");
    printf("case=v1-atu-mixed init=false bytes_unchanged=%s version=%d\n",
           before == after ? "true" : "false", query_int(PARAMS_DB, "SELECT id FROM version"));
}

/* Esquema incompleto: no se migra ni se inventa lo que falta. */
static void case_schema_incomplete(void) {
    uint64_t before = 0, after = 0;

    check(file_hash(PARAMS_DB, &before), "hash previo");
    check(!database_init(), "database_init() falla con esquema incompleto");
    check(file_hash(PARAMS_DB, &after), "hash posterior");
    check(before == after, "la base incompleta queda intacta");
    printf("case=schema-incomplete init=false bytes_unchanged=%s\n", before == after ? "true" : "false");
}

/* Version sin fila: estado ambiguo -> aborta. */
static void case_version_missing(void) {
    uint64_t before = 0, after = 0;

    check(file_hash(PARAMS_DB, &before), "hash previo");
    check(!database_init(), "database_init() falla sin fila de version");
    check(file_hash(PARAMS_DB, &after), "hash posterior");
    check(before == after, "la base queda intacta");
    printf("case=version-missing init=false bytes_unchanged=%s\n", before == after ? "true" : "false");
}

/* --- main ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("uso: %s <caso>|--make-db <fixture>|--make-template <t>\n", argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "--make-db") == 0) {
        if (argc < 3) {
            return 2;
        }

        db_fixture_t kind  = DB_FIX_VALID;
        bool         found = false;

        for (int k = 0; k <= (int) DB_FIX_TEMPLATE_BAD; k++) {
            if (strcmp(argv[2], db_fixture_name((db_fixture_t) k)) == 0) {
                kind  = (db_fixture_t) k;
                found = true;
                break;
            }
        }
        if (!found) {
            printf("fixture desconocido: %s\n", argv[2]);
            return 2;
        }
        if (!db_fixture_make(PARAMS_DB, kind)) {
            printf("no se pudo crear el fixture\n");
            return 1;
        }
        printf("fixture=%s\n", db_fixture_name(kind));
        return 0;
    }

    if (strcmp(argv[1], "--make-template") == 0) {
        if (argc < 3) {
            return 2;
        }
        if (strcmp(argv[2], "none") == 0) {
            unlink(TEMPLATE_DB);
        } else if (strcmp(argv[2], "valid") == 0) {
            if (!db_fixture_make(TEMPLATE_DB, DB_FIX_VALID)) {
                return 1;
            }
        } else if (strcmp(argv[2], "bad") == 0) {
            if (!db_fixture_make(TEMPLATE_DB, DB_FIX_TEMPLATE_BAD)) {
                return 1;
            }
        } else {
            printf("plantilla desconocida: %s\n", argv[2]);
            return 2;
        }
        printf("template=%s\n", argv[2]);
        return 0;
    }

    const char *c = argv[1];

    if (strcmp(c, "valid") == 0) {
        case_valid();
    } else if (strcmp(c, "absent") == 0) {
        case_absent();
    } else if (strcmp(c, "absent-no-template") == 0) {
        case_absent_no_template();
    } else if (strcmp(c, "no-mount") == 0) {
        case_no_mount();
    } else if (strcmp(c, "corrupt") == 0) {
        case_corrupt();
    } else if (strcmp(c, "ro") == 0) {
        case_ro();
    } else if (strcmp(c, "full") == 0) {
        case_full();
    } else if (strcmp(c, "v1-atu") == 0) {
        case_v1_atu();
    } else if (strcmp(c, "v1-atu-mixed") == 0) {
        case_v1_atu_mixed();
    } else if (strcmp(c, "schema-incomplete") == 0) {
        case_schema_incomplete();
    } else if (strcmp(c, "version-missing") == 0) {
        case_version_missing();
    } else {
        printf("caso desconocido: %s\n", c);
        return 2;
    }

    printf("case=%s result=%s\n", c, failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

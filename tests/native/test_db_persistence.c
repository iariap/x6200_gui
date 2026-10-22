/*
 * Persistencia fail-closed de params.db (issue #213), en el host y sin radio.
 *
 * Cubre, con SQLite REAL y fixtures propios:
 *   - la decision de la guarda de montaje de DATA (formato /proc/self/mountinfo)
 *     contra fixtures del formato: montaje rw y de otro filesystem, ausente, en
 *     RO y sobre el rootfs (mismo dispositivo que /);
 *   - la validacion de una base y de la plantilla (integridad quick_check +
 *     esquema base): valida, corrupta, truncada, vacia, esquema incompleto;
 *   - la instalacion de la plantilla (bytes identicos, 0644, sin pisar una base
 *     existente, sin temporales, plantilla invalida -> no instala);
 *   - las migraciones: secuencia en UNA transaccion, version sin subir ante
 *     fallo, base al dia intacta, version ausente/ambigua rechazada y la guarda
 *     de la migracion 2 del ATU (indice -> Hz una sola vez; mezcla -> aborta sin
 *     convertir);
 *   - la propagacion del error de escritura: `dirty` solo se limpia con
 *     SQLITE_DONE, el fallo se reporta una vez por racha y no se pierde el dato.
 *
 * Lo que necesita un /mnt DE VERDAD (montaje ausente/RO, base llena, y el
 * arranque del binario ARM) NO se simula aca: esta en
 * tests/native/run_db_failclosed.sh (namespace/chroot del host) y en la suite
 * QEMU con el binario de la ROM.
 *
 * Este test incluye src/params/db.c para llegar a sus funciones internas (mismo
 * patron que tests/native/test_remote.cpp con src/remote_server.cpp). La parte de
 * migraciones linkea src/params/migrations.c (staged) tal cual sale del repo.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test_framework.h"
#include "db_test_stubs.h"
#include "db_fixtures.h"

#include "../../src/params/db.c"

/* --- utilidades de archivos ------------------------------------------------ */

static char sandbox[PATH_MAX];

/* Copia `src` en `dst` sin snprintf: con buffers de PATH_MAX el compilador no
 * puede saber que la ruta entra y avisa por -Wformat-truncation. */
static void copy_str(char *dst, size_t dst_size, const char *src) {
    size_t len = strlen(src);

    CHECK(len + 1 <= dst_size);
    memcpy(dst, src, len + 1);
}

static void path_in(char *out, size_t out_size, const char *name) {
    size_t dir_len = strlen(sandbox);
    size_t len     = strlen(name);

    CHECK(dir_len + 1 + len + 1 <= out_size);
    memcpy(out, sandbox, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1, name, len + 1);
}

static void path_join(char *out, size_t out_size, const char *dir, const char *name) {
    size_t dir_len = strlen(dir);
    size_t len     = strlen(name);

    CHECK(dir_len + 1 + len + 1 <= out_size);
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1, name, len + 1);
}

/* FNV-1a de 64 bits: alcanza para comparar bytes de un archivo entre dos
 * momentos (el test no necesita criptografia). */
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

static bool file_size(const char *path, long *size) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return false;
    }
    *size = (long) st.st_size;
    return true;
}

static bool file_mode(const char *path, mode_t *mode) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return false;
    }
    *mode = st.st_mode & 07777;
    return true;
}

static void write_bytes(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");

    if (f == NULL) {
        CHECK(false);
        return;
    }
    CHECK(fwrite(data, 1, len, f) == len);
    fclose(f);
}

/* Cuenta los archivos temporales que pueda haber dejado una instalacion a
 * medias (`params.db.tmp*`) en el directorio de la base. */
static int temp_leftovers(const char *dir) {
    DIR           *d     = opendir(dir);
    struct dirent *entry = NULL;
    int            count = 0;

    if (d == NULL) {
        return -1;
    }
    while ((entry = readdir(d)) != NULL) {
        if (strncmp(entry->d_name, "params.db.tmp", 13) == 0) {
            count++;
        }
    }
    closedir(d);
    return count;
}

static bool db_has_table(const char *path, const char *table) {
    sqlite3      *h    = NULL;
    sqlite3_stmt *stmt = NULL;
    bool          has  = false;

    if (sqlite3_open_v2(path, &h, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        sqlite3_close(h);
        return false;
    }
    if (sqlite3_prepare_v2(h, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?",
                           -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, table, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            has = sqlite3_column_int(stmt, 0) > 0;
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(h);
    return has;
}

static int db_query_int(const char *path, const char *sql) {
    sqlite3      *h    = NULL;
    sqlite3_stmt *stmt = NULL;
    int           val  = INT_MIN;

    if (sqlite3_open_v2(path, &h, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        sqlite3_close(h);
        return INT_MIN;
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

static int db_atu_freq(const char *path, int ant, int val) {
    sqlite3      *h    = NULL;
    sqlite3_stmt *stmt = NULL;
    int           freq = INT_MIN;

    if (sqlite3_open_v2(path, &h, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        sqlite3_close(h);
        return INT_MIN;
    }
    if (sqlite3_prepare_v2(h, "SELECT freq FROM atu WHERE ant=? AND val=?", -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, ant);
        sqlite3_bind_int(stmt, 2, val);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            freq = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(h);
    return freq;
}

/* --- fixtures de base ------------------------------------------------------- */
/*
 * Los fixtures (y el esquema que copian de sql/params.sql) viven en
 * tests/native/db_fixtures.c: los comparte el end-to-end con las rutas reales
 * para que la base medida sea la misma en los dos lados.
 */

/* --- A. guarda de montaje de DATA ------------------------------------------ */

static void test_mount_guard(void) {
    char info[PATH_MAX];
    char why[256];

    path_in(info, sizeof info, "mountinfo");

    static const char *const RW_SEPARATE =
        "26 1 8:2 / / rw,relatime shared:1 - ext4 /dev/mmcblk0p2 rw\n"
        "31 26 8:3 / /mnt rw,relatime shared:2 - ext4 /dev/mmcblk0p3 rw\n";
    static const char *const NOT_MOUNTED =
        "26 1 8:2 / / rw,relatime shared:1 - ext4 /dev/mmcblk0p2 rw\n"
        "27 26 0:40 / /dev/shm rw,nosuid,nodev - tmpfs tmpfs rw\n";
    static const char *const READ_ONLY =
        "26 1 8:2 / / rw,relatime shared:1 - ext4 /dev/mmcblk0p2 rw\n"
        "31 26 8:3 / /mnt ro,relatime shared:2 - ext4 /dev/mmcblk0p3 rw\n";
    static const char *const SAME_ROOTFS =
        "26 1 8:2 / / rw,relatime shared:1 - ext4 /dev/mmcblk0p2 rw\n"
        "31 26 8:2 /mnt /mnt rw,relatime shared:2 - ext4 /dev/mmcblk0p2 rw\n";

    write_bytes(info, RW_SEPARATE, strlen(RW_SEPARATE));
    why[0] = 0;
    CHECK(mount_entry_ok(info, "/mnt", why, sizeof why));

    write_bytes(info, NOT_MOUNTED, strlen(NOT_MOUNTED));
    why[0] = 0;
    CHECK(!mount_entry_ok(info, "/mnt", why, sizeof why));
    CHECK(strstr(why, "not a mount point") != NULL);

    write_bytes(info, READ_ONLY, strlen(READ_ONLY));
    why[0] = 0;
    CHECK(!mount_entry_ok(info, "/mnt", why, sizeof why));
    CHECK(strstr(why, "read-only") != NULL);

    write_bytes(info, SAME_ROOTFS, strlen(SAME_ROOTFS));
    why[0] = 0;
    CHECK(!mount_entry_ok(info, "/mnt", why, sizeof why));
    CHECK(strstr(why, "root filesystem") != NULL);

    /* Sin /proc/self/mountinfo (o vacio) no hay montaje comprobable. */
    write_bytes(info, "", 0);
    why[0] = 0;
    CHECK(!mount_entry_ok(info, "/mnt", why, sizeof why));

    unlink(info);
}

/* --- B. validacion de una base y de la plantilla ---------------------------- */

static void test_file_validation(void) {
    char valid[PATH_MAX];
    char path[PATH_MAX];
    char why[256];

    path_in(valid, sizeof valid, "valid.db");
    CHECK(db_fixture_make(valid, DB_FIX_VALID));

    why[0] = 0;
    CHECK(db_file_valid(valid, why, sizeof why));

    /* Corrupta: bytes que no son una base. */
    char corrupt[PATH_MAX];

    path_in(corrupt, sizeof corrupt, "corrupt.db");
    static const char garbage[] = "esto no es una base sqlite, ni cerca lo suficiente";

    write_bytes(corrupt, garbage, sizeof garbage - 1);
    why[0] = 0;
    CHECK(!db_file_valid(corrupt, why, sizeof why));

    /* Vacia (0 bytes): no tiene esquema. */
    char empty[PATH_MAX];

    path_in(empty, sizeof empty, "empty.db");
    write_bytes(empty, "", 0);
    why[0] = 0;
    CHECK(!db_file_valid(empty, why, sizeof why));

    /* Truncada: cabecera valida y el resto cortado. */
    char truncated[PATH_MAX];

    path_in(truncated, sizeof truncated, "truncated.db");
    CHECK(db_fixture_make(truncated, DB_FIX_VALID));

    long size = 0;

    CHECK(file_size(truncated, &size));
    CHECK(size > 4096);
    CHECK(truncate(truncated, 4096) == 0);
    why[0] = 0;
    CHECK(!db_file_valid(truncated, why, sizeof why));

    /* Esquema incompleto: le falta una tabla base. */
    char no_bands[PATH_MAX];

    path_in(no_bands, sizeof no_bands, "no-bands.db");
    CHECK(db_fixture_make(no_bands, DB_FIX_NO_BANDS));
    why[0] = 0;
    CHECK(!db_file_valid(no_bands, why, sizeof why));
    CHECK(strstr(why, "missing table bands") != NULL);

    /* Ausente: no hay nada que validar. */
    path_in(path, sizeof path, "nope.db");
    unlink(path);
    why[0] = 0;
    CHECK(!db_file_valid(path, why, sizeof why));

    unlink(valid);
    unlink(corrupt);
    unlink(empty);
    unlink(truncated);
    unlink(no_bands);
}

/* --- C. instalacion de la plantilla ---------------------------------------- */

static void test_template_install(void) {
    char dir[PATH_MAX];
    char tmpl[PATH_MAX];
    char target[PATH_MAX];
    char why[256];

    path_in(dir, sizeof dir, "install");
    path_join(tmpl, sizeof tmpl, dir, "template.db");
    path_join(target, sizeof target, dir, "params.db");

    mkdir(dir, 0755);
    unlink(target);
    CHECK(db_fixture_make(tmpl, DB_FIX_VALID));

    /* Plantilla valida + destino ausente: se instala igual a la plantilla. */
    why[0] = 0;
    CHECK(db_install_template(target, tmpl, why, sizeof why));
    CHECK(db_has_table(target, "bands"));

    uint64_t h_src = 0, h_dst = 0;

    CHECK(file_hash(tmpl, &h_src));
    CHECK(file_hash(target, &h_dst));
    CHECK(h_src == h_dst);

    mode_t mode = 0;

    CHECK(file_mode(target, &mode));
    CHECK(mode == 0644);
    CHECK(temp_leftovers(dir) == 0);

    /* Destino existente: NO se pisa (una base que aparecio en paralelo se
     * respeta), aunque su contenido sea distinto. */
    static const char other[] = "otra cosa";

    write_bytes(target, other, sizeof other - 1);
    why[0] = 0;
    CHECK(db_install_template(target, tmpl, why, sizeof why));

    uint64_t h_after = 0;

    CHECK(file_hash(target, &h_after));
    CHECK(h_after != h_src);
    long size = 0;

    CHECK(file_size(target, &size));
    CHECK(size == (long) (sizeof other - 1));
    CHECK(temp_leftovers(dir) == 0);

    unlink(target);

    /* Plantilla invalida: no se instala nada ni quedan temporales. */
    char bad_tmpl[PATH_MAX];

    path_join(bad_tmpl, sizeof bad_tmpl, dir, "bad-template.db");
    CHECK(db_fixture_make(bad_tmpl, DB_FIX_TEMPLATE_BAD));
    why[0] = 0;
    CHECK(!db_install_template(target, bad_tmpl, why, sizeof why));
    CHECK(access(target, F_OK) != 0);
    CHECK(temp_leftovers(dir) == 0);

    /* Plantilla ausente. */
    char missing_tmpl[PATH_MAX];

    path_join(missing_tmpl, sizeof missing_tmpl, dir, "nope.db");
    why[0] = 0;
    CHECK(!db_install_template(target, missing_tmpl, why, sizeof why));
    CHECK(access(target, F_OK) != 0);

    /* Directorio inexistente: no puede instalar con exclusion segura. */
    char bad_target[PATH_MAX];

    path_join(bad_target, sizeof bad_target, dir, "no-existe/params.db");
    why[0] = 0;
    CHECK(!db_install_template(bad_target, tmpl, why, sizeof why));

    unlink(tmpl);
    unlink(bad_tmpl);
    unlink(target);
    rmdir(dir);
}

/* --- D. migraciones -------------------------------------------------------- */

static bool db_open_rw(const char *path) {
    db = NULL;
    return sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK;
}

static void db_close(void) {
    if (db != NULL) {
        sqlite3_close(db);
        db = NULL;
    }
}

static void test_migrations_healthy(void) {
    char path[PATH_MAX];

    path_in(path, sizeof path, "healthy.db");
    CHECK(db_fixture_make(path, DB_FIX_VALID));

    uint64_t before = 0, after = 0;

    CHECK(file_hash(path, &before));
    CHECK(db_open_rw(path));
    CHECK(migrations_apply() == 0);
    db_close();
    CHECK(file_hash(path, &after));
    /* Una base al dia no se toca: ni una escritura. */
    CHECK(before == after);
    CHECK(db_query_int(path, "SELECT id FROM version") == 5);
    unlink(path);
}

static void test_migrations_atu(void) {
    char path[PATH_MAX];
    uint64_t before = 0, after = 0;

    /* Forma vieja (indices): se convierte una vez y la version sube a 5. */
    path_in(path, sizeof path, "atu-index.db");
    CHECK(db_fixture_make(path, DB_FIX_V1_ATU_INDEX));
    CHECK(db_open_rw(path));
    CHECK(migrations_apply() == 0);
    /* Reintentar la migracion (ya al dia) no puede volver a convertir. */
    CHECK(migrations_apply() == 0);
    db_close();
    CHECK(db_query_int(path, "SELECT id FROM version") == 5);
    CHECK(db_atu_freq(path, 1, 100) == 3 * 50000 + 25000);
    CHECK(db_atu_freq(path, 1, 200) == 5 * 50000 + 25000);
    unlink(path);

    /* Ya en Hz (version 1, migracion 2 ya aplicada de hecho): NADA se convierte. */
    path_in(path, sizeof path, "atu-hz.db");
    CHECK(db_fixture_make(path, DB_FIX_V1_ATU_HZ));
    CHECK(db_open_rw(path));
    CHECK(migrations_apply() == 0);
    db_close();
    CHECK(db_query_int(path, "SELECT id FROM version") == 5);
    CHECK(db_atu_freq(path, 1, 100) == 7025000);
    CHECK(db_atu_freq(path, 1, 200) == 14250000);
    unlink(path);

    /* Mezcla: procedencia ambigua -> aborta, no convierte y deja la base igual. */
    path_in(path, sizeof path, "atu-mixed.db");
    CHECK(db_fixture_make(path, DB_FIX_V1_ATU_MIXED));
    CHECK(file_hash(path, &before));
    CHECK(db_open_rw(path));
    CHECK(migrations_apply() != 0);
    db_close();
    CHECK(db_query_int(path, "SELECT id FROM version") == 1);
    CHECK(db_atu_freq(path, 1, 100) == 3);
    CHECK(db_atu_freq(path, 1, 200) == 7025000);
    CHECK(db_query_int(path, "SELECT count(*) FROM atu") == 2);
    CHECK(file_hash(path, &after));
    /* El ROLLBACK deja los bytes originales: no quedo nada a medias. */
    CHECK(before == after);
    unlink(path);
}

static void test_migrations_refusals(void) {
    char path[PATH_MAX];

    /* Version ambigua: la tabla existe pero sin fila. */
    path_in(path, sizeof path, "no-version.db");
    CHECK(db_fixture_make(path, DB_FIX_NO_VERSION_ROW));
    CHECK(db_open_rw(path));
    CHECK(migrations_apply() != 0);
    db_close();
    CHECK(db_query_int(path, "SELECT count(*) FROM version") == 0);
    CHECK(db_atu_freq(path, 1, 120) == 7025000);   /* la base sigue entera */
    unlink(path);

    /* Version mas nueva que el firmware: no se migra hacia atras. */
    path_in(path, sizeof path, "future.db");
    CHECK(db_fixture_make(path, DB_FIX_BAD_VERSION));
    CHECK(db_open_rw(path));
    CHECK(migrations_apply() != 0);
    db_close();
    CHECK(db_query_int(path, "SELECT id FROM version") == 9);
    unlink(path);

    /* Migracion que falla (choque de UNIQUE en _1): la version NO sube y la
     * base queda como estaba (una sola transaccion). */
    path_in(path, sizeof path, "conflict.db");
    CHECK(db_fixture_make(path, DB_FIX_V0_CONFLICT));
    CHECK(db_open_rw(path));
    CHECK(migrations_apply() != 0);
    db_close();
    CHECK(db_query_int(path, "SELECT id FROM version") == 0);
    CHECK(db_query_int(path, "SELECT count(*) FROM digital_modes") == 1);
    CHECK(db_query_int(path, "SELECT freq FROM digital_modes") == 14074000);
    unlink(path);
}

/* --- E. escrituras --------------------------------------------------------- */

static bool db_prepare_insert(const char *path, bool readonly) {
    db = NULL;
    insert_stmt = NULL;

    int flags = readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;

    if (sqlite3_open_v2(path, &db, flags, NULL) != SQLITE_OK) {
        return false;
    }
    return sqlite3_prepare_v2(db, "INSERT INTO params(name, val) VALUES(?, ?)", -1, &insert_stmt, 0) == SQLITE_OK;
}

static void test_write_success(void) {
    char path[PATH_MAX];

    path_in(path, sizeof path, "write-ok.db");
    CHECK(db_fixture_make(path, DB_FIX_VALID));

    CHECK(db_prepare_insert(path, false));
    db_test_log_reset();

    bool dirty = true;

    params_write_int("probe", 4242, &dirty);
    CHECK(!dirty);
    CHECK(db_test_log_lines_count() == 0);
    CHECK(db_query_int(path, "SELECT val FROM params WHERE name='probe'") == 4242);

    db_close();
    unlink(path);
}

static void test_write_failure(void) {
    char path[PATH_MAX];

    path_in(path, sizeof path, "write-ro.db");
    CHECK(db_fixture_make(path, DB_FIX_VALID));

    /* Conexion READONLY: el INSERT se puede preparar pero falla al ejecutar,
     * que es exactamente el error que el codigo viejo descartaba. */
    CHECK(db_prepare_insert(path, true));
    db_test_log_reset();

    bool dirty = true;

    params_write_int("probe", 1, &dirty);
    CHECK(dirty);  /* el dato NO quedo persistido: sigue en memoria */
    CHECK(db_test_log_occurrences("Persistence write failed") == 1);

    /* Sin spam por tick: dos intentos mas, el mismo rc, un solo aviso. */
    params_write_int("probe", 2, &dirty);
    params_write_int("probe", 3, &dirty);
    CHECK(dirty);
    CHECK(db_test_log_occurrences("Persistence write failed") == 1);
    CHECK(db_query_int(path, "SELECT count(*) FROM params WHERE name='probe'") == 0);

    db_close();

    /* Exito despues de un fallo -> se rearma el aviso. */
    CHECK(db_prepare_insert(path, false));
    params_write_int("probe", 7, &dirty);
    CHECK(!dirty);
    CHECK(db_test_log_occurrences("Persistence write failed") == 1);
    CHECK(db_query_int(path, "SELECT val FROM params WHERE name='probe'") == 7);

    db_close();
    CHECK(db_prepare_insert(path, true));
    dirty = true;
    params_write_int("probe", 8, &dirty);
    CHECK(dirty);
    CHECK(db_test_log_occurrences("Persistence write failed") == 2);
    db_close();

    /* Sin statement preparado (arranque fallido): no crashea, conserva dirty y
     * lo reporta. */
    db = NULL;
    insert_stmt = NULL;
    db_test_log_reset();
    dirty = true;
    params_write_int("probe", 9, &dirty);
    CHECK(dirty);
    CHECK(db_test_log_occurrences("Persistence write failed") == 1);

    unlink(path);
}

static void test_query_exec_without_db(void) {
    db = NULL;

    /* No puede crashear ni perder el error cuando la base no esta abierta. */
    CHECK(!sql_query_exec("SELECT 1"));
}

/* --- main ------------------------------------------------------------------ */

int main(void) {
    const char *dir = getenv("DB_PERSIST_DIR");

    if (dir == NULL || dir[0] == 0) {
        printf("FALLO: falta DB_PERSIST_DIR (directorio privado y vacio para los fixtures)\n");
        return 2;
    }
    copy_str(sandbox, sizeof sandbox, dir);

    test_mount_guard();
    test_file_validation();
    test_template_install();
    test_migrations_healthy();
    test_migrations_atu();
    test_migrations_refusals();
    test_write_success();
    test_write_failure();
    test_query_exec_without_db();

    printf("%d comprobaciones, %d fallos\n", tests_run, tests_failed);

    return tests_failed ? 1 : 0;
}

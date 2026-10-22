/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2024 Georgy Dyuldin aka R2RFE
 */

#include "db.h"

#include "migrations.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <lvgl/src/misc/lv_log.h>

/* Parametros de la persistencia (issue #213).
 *
 * `params.db` vive en la particion DATA montada en /mnt: si /mnt no es un
 * montaje REAL y en `rw`, sqlite3 abriria (o crearia) la base sobre el rootfs.
 * Eso hoy es invisible: el archivo queda en el sistema de archivos efimero, las
 * escrituras "funcionan" y los ajustes se pierden en el proximo arranque. Por eso
 * el arranque es fail-closed: antes de tocar la base se comprueba el montaje.
 *
 * La plantilla versionada la instala el paquete (sql/params.sql via CMake) como
 * /usr/share/x6200/params.default.db; es la UNICA fuente valida de una base
 * nueva (nunca se crea un esquema vacio). */
#define DB_PARAMS_PATH      "/mnt/params.db"
#define DB_TEMPLATE_PATH    "/usr/share/x6200/params.default.db"
#define DB_MOUNTINFO_PATH   "/proc/self/mountinfo"

/* Tablas base que tiene que traer una base valida ANTES de migrar (contrato del
 * issue #213, punto 4). Las que agregan las migraciones vigentes se comprueban
 * despues de migrar (DB_REQUIRED_TABLES). */
static const char *const DB_BASE_TABLES[] = {
    "version", "params", "bands", "band_params", "mode_params", "memory",
};

/* Tablas que tiene que tener la base ya migrada: las base mas las que crean o
 * necesitan las migraciones vigentes (_1 digital_modes, _2 atu, _5 dfl_*), que
 * tambien son las que consultan cfg_atu_init()/cfg_digital_modes_init()/
 * dfl_presets_store al arrancar. */
static const char *const DB_REQUIRED_TABLES[] = {
    "version", "params", "bands", "band_params", "mode_params", "memory",
    "atu", "digital_modes", "dfl_filter_memories", "dfl_filter_favorites",
};

#define DB_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

sqlite3                 *db = NULL;

static sqlite3_stmt     *insert_stmt;


static void errorLogCallback(void *pArg, int iErrCode, const char *zMsg){
    LV_LOG_ERROR("(%d) %s\n", iErrCode, zMsg);
}

static void db_close_resources(void) {
    if (insert_stmt != NULL) {
        sqlite3_finalize(insert_stmt);
        insert_stmt = NULL;
    }
    if (db != NULL) {
        sqlite3_close(db);
        db = NULL;
    }
}

/* Directorio de un path absoluto ("/mnt/params.db" -> "/mnt"). Falla si el path
 * no tiene directorio (nada de escribir sobre el rootfs por accidente). */
static bool path_dir(const char *path, char *dir, size_t dir_size) {
    const char *slash = strrchr(path, '/');

    if (slash == NULL || slash == path || (size_t) (slash - path) + 1 > dir_size) {
        return false;
    }
    memcpy(dir, path, (size_t) (slash - path));
    dir[slash - path] = 0;
    return true;
}

static bool has_mount_option(const char *opts, const char *want) {
    size_t wlen = strlen(want);

    for (const char *p = opts; *p != 0; ) {
        const char *end = strchr(p, ',');
        size_t      len = end != NULL ? (size_t) (end - p) : strlen(p);

        if (len == wlen && strncmp(p, want, wlen) == 0) {
            return true;
        }
        if (end == NULL) {
            break;
        }
        p = end + 1;
    }
    return false;
}

/* El directorio de la base (`/mnt`) tiene que ser un montaje real, de un
 * filesystem distinto al de la raiz y en `rw`. `mountinfo` sale de
 * /proc/self/mountinfo (linea: id parent major:minor root mountpoint opts
 * [opcionales...] "-" fstype source superopts); el campo de opciones por montaje
 * es donde el kernel dice `rw`/`ro` de forma confiable. La ruta del archivo es un
 * parametro para poder fijar la decision con fixtures del formato en los tests
 * (tests/native/test_db_persistence.c); en el firmware siempre es /proc. */
static bool mount_entry_ok(const char *mountinfo, const char *dir, char *why, size_t why_size) {
    FILE *f = fopen(mountinfo, "r");

    if (f == NULL) {
        snprintf(why, why_size, "cannot read %.160s: %.64s", mountinfo, strerror(errno));
        return false;
    }

    bool found      = false;
    bool rw         = false;
    bool root_found = false;
    char dir_dev[64]  = "";
    char root_dev[64] = "";
    char fs_type[64]  = "";
    char source[256]  = "";
    char line[4096];

    while (fgets(line, sizeof line, f) != NULL) {
        /* Campos: id parent major:minor root mountpoint opts [opcionales...]
         * "-" fstype source superopts. El kernel escapa los espacios de los
         * paths (\040), asi que separar por ' ' es seguro. */
        char *field[6];
        char *p       = line;
        bool  parsed  = true;

        for (int i = 0; i < 6; i++) {
            field[i] = p;

            char *sp = strchr(p, ' ');

            if (sp == NULL) {
                parsed = false;
                break;
            }
            *sp = 0;
            p   = sp + 1;
        }
        if (!parsed) {
            continue;
        }

        if (strcmp(field[4], dir) == 0) {
            found = true;
            snprintf(dir_dev, sizeof dir_dev, "%s", field[2]);
            rw = has_mount_option(field[5], "rw");

            char *dash = strstr(p, "- ");

            if (dash != NULL) {
                snprintf(fs_type, sizeof fs_type, "%63s", dash + 2);

                char *sp = strchr(dash + 2, ' ');

                if (sp != NULL) {
                    snprintf(source, sizeof source, "%255s", sp + 1);
                }
            }
        } else if (strcmp(field[4], "/") == 0) {
            root_found = true;
            snprintf(root_dev, sizeof root_dev, "%s", field[2]);
        }
    }
    fclose(f);

    if (!found) {
        snprintf(why, why_size, "%.160s is not a mount point (DATA is not mounted)", dir);
        return false;
    }
    if (!rw) {
        snprintf(why, why_size, "%.160s is mounted read-only", dir);
        return false;
    }
    /* Un bind mount de un directorio del rootfs aparece como montaje pero no
     * lleva los datos de DATA: mismo dispositivo que la raiz. */
    if (root_found && strcmp(dir_dev, root_dev) == 0) {
        snprintf(why, why_size, "%.140s is the root filesystem (%.48s), not DATA", dir,
                 source[0] != 0 ? source : fs_type);
        return false;
    }
    return true;
}

static bool data_mount_ok(const char *dir, char *why, size_t why_size) {
    return mount_entry_ok(DB_MOUNTINFO_PATH, dir, why, why_size);
}

/* PRAGMA quick_check(1): "ok" es lo unico que vale. Detecta una base corrupta o
 * truncada antes de migrar o de que la GUI la use. */
static bool db_quick_check(sqlite3 *handle, char *why, size_t why_size) {
    sqlite3_stmt *stmt  = NULL;
    bool          ok    = false;

    if (sqlite3_prepare_v2(handle, "PRAGMA quick_check(1)", -1, &stmt, 0) != SQLITE_OK) {
        snprintf(why, why_size, "quick_check could not run: %.64s", sqlite3_errmsg(handle));
        return false;
    }

    int rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        const char *result = (const char *) sqlite3_column_text(stmt, 0);

        ok = result != NULL && strcmp(result, "ok") == 0;
        if (!ok) {
            snprintf(why, why_size, "quick_check: %.64s", result != NULL ? result : "(no result)");
        }
    } else {
        snprintf(why, why_size, "quick_check failed: %.64s", sqlite3_errmsg(handle));
    }
    sqlite3_finalize(stmt);
    return ok;
}

static bool db_table_exists(sqlite3 *handle, const char *name) {
    sqlite3_stmt *stmt   = NULL;
    bool          exists = false;

    if (sqlite3_prepare_v2(handle, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?",
                           -1, &stmt, 0) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }
    sqlite3_finalize(stmt);
    return exists;
}

static bool db_tables_present(sqlite3 *handle, const char *const *tables, size_t count,
                              const char *what, char *why, size_t why_size) {
    for (size_t i = 0; i < count; i++) {
        if (!db_table_exists(handle, tables[i])) {
            snprintf(why, why_size, "%.80s: missing table %.64s", what, tables[i]);
            return false;
        }
    }
    return true;
}

/* Una base valida: se puede abrir, esta integra y tiene el esquema base. Se usa
 * READONLY y se cierra enseguida: validar no puede modificar nada. */
static bool db_file_valid(const char *path, char *why, size_t why_size) {
    sqlite3 *handle = NULL;
    int      rc     = sqlite3_open_v2(path, &handle, SQLITE_OPEN_READONLY, NULL);

    if (rc != SQLITE_OK) {
        snprintf(why, why_size, "cannot open %.160s: %.64s", path,
                 handle != NULL ? sqlite3_errmsg(handle) : sqlite3_errstr(rc));
        sqlite3_close(handle);
        return false;
    }

    bool ok = db_quick_check(handle, why, why_size) &&
              db_tables_present(handle, DB_BASE_TABLES, DB_ARRAY_COUNT(DB_BASE_TABLES),
                                path, why, why_size);

    sqlite3_close(handle);
    return ok;
}

static bool copy_fd(int src, int dst) {
    char buf[65536];

    for (;;) {
        ssize_t n = read(src, buf, sizeof buf);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return true;
        }

        ssize_t off = 0;

        while (off < n) {
            ssize_t w = write(dst, buf + off, (size_t) (n - off));

            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return false;
            }
            off += w;
        }
    }
}

static void fsync_dir(const char *dir) {
    int fd = open(dir, O_RDONLY | O_DIRECTORY);

    if (fd < 0) {
        return;
    }
    (void) fsync(fd);
    close(fd);
}

/* Instala la plantilla versionada en `path` sin pisar nada: archivo temporal en
 * el MISMO directorio (para que el link sea atomico), 0644, fsync del archivo y
 * link() al destino -- link() falla con EEXIST, a diferencia de rename(), asi que
 * una base que aparezca en paralelo queda intacta -- y fsync del directorio. Si
 * la instalacion no se puede hacer con exclusion segura, falla. */
static bool db_install_template(const char *path, const char *tmpl, char *why, size_t why_size) {
    char dir[PATH_MAX];

    if (!path_dir(path, dir, sizeof dir)) {
        snprintf(why, why_size, "cannot resolve the directory of %.160s", path);
        return false;
    }
    if (!db_file_valid(tmpl, why, why_size)) {
        return false;
    }

    char tmp[PATH_MAX];

    if ((size_t) snprintf(tmp, sizeof tmp, "%s/params.db.tmpXXXXXX", dir) >= sizeof tmp) {
        snprintf(why, why_size, "temp path too long in %.160s", dir);
        return false;
    }

    int tmp_fd = mkstemp(tmp);

    if (tmp_fd < 0) {
        snprintf(why, why_size, "cannot create %.160s: %.64s", tmp, strerror(errno));
        return false;
    }

    int src_fd = open(tmpl, O_RDONLY);

    if (src_fd < 0) {
        snprintf(why, why_size, "cannot read %.160s: %.64s", tmpl, strerror(errno));
        close(tmp_fd);
        unlink(tmp);
        return false;
    }

    bool ok = copy_fd(src_fd, tmp_fd);

    close(src_fd);

    /* La base es del root (asi corre la GUI en la radio) y 0644: nunca
     * world-writable. Sin privilegios (build de prueba del host) fchown() falla
     * con EPERM y el archivo queda del usuario que corre la GUI: no es fatal,
     * pero el valor de retorno NO se ignora (glibc lo marca warn_unused_result) y
     * cualquier otro error se reporta. */
    if (ok) {
        ok = fchmod(tmp_fd, 0644) == 0;
    }
    if (ok) {
        errno = 0;
        if (fchown(tmp_fd, 0, 0) != 0 && errno != EPERM) {
            LV_LOG_WARN("params.db: cannot make %.160s root-owned: %.64s", tmp, strerror(errno));
        }
    }
    if (ok) {
        ok = fsync(tmp_fd) == 0;
    }
    if (!ok) {
        snprintf(why, why_size, "writing %.160s: %.64s", tmp, strerror(errno));
        close(tmp_fd);
        unlink(tmp);
        return false;
    }
    if (close(tmp_fd) != 0) {
        snprintf(why, why_size, "closing %.160s: %.64s", tmp, strerror(errno));
        unlink(tmp);
        return false;
    }

    if (link(tmp, path) != 0) {
        int e = errno;

        unlink(tmp);
        if (e != EEXIST) {
            snprintf(why, why_size, "cannot install %.160s: %.64s", path, strerror(e));
            return false;
        }
        /* Una base aparecio entre el test de ausencia y el link: se respeta. */
        return true;
    }
    /* El nombre temporal solo servia para el link: la base es `path`. */
    unlink(tmp);
    fsync_dir(dir);
    return true;
}

/* Arranque fail-closed de la persistencia (issue #213). Devuelve false -- sin
 * dejar `db` abierta ni recursos a medias -- si DATA no es utilizable, si la
 * base no se puede validar, si el esquema base esta incompleto o si las
 * migraciones fallan. Nunca repara, renombra ni borra una base existente. */
bool database_init(void) {
    char why[256] = "";
    char dir[PATH_MAX];

    sqlite3_config(SQLITE_CONFIG_LOG, errorLogCallback, NULL);
    sqlite3_config(SQLITE_CONFIG_SERIALIZED);

    if (!path_dir(DB_PARAMS_PATH, dir, sizeof dir)) {
        LV_LOG_ERROR("params.db: bad path %s", DB_PARAMS_PATH);
        return false;
    }
    if (!data_mount_ok(dir, why, sizeof why)) {
        LV_LOG_ERROR("params.db: DATA unavailable: %s", why);
        return false;
    }

    if (access(DB_PARAMS_PATH, F_OK) != 0) {
        if (errno != ENOENT) {
            LV_LOG_ERROR("params.db: cannot stat %s: %s", DB_PARAMS_PATH, strerror(errno));
            return false;
        }

        /* Base ausente en DATA valida: se siembra la plantilla versionada. Nunca
         * se crea un esquema vacio y se sigue (seria una base sin datos). */
        LV_LOG_WARN("params.db: absent, seeding from %s", DB_TEMPLATE_PATH);

        if (!db_install_template(DB_PARAMS_PATH, DB_TEMPLATE_PATH, why, sizeof why)) {
            LV_LOG_ERROR("params.db: cannot seed the template: %s", why);
            return false;
        }
    }

    /* READWRITE SIN CREATE: la base tiene que existir (recien instalada o
     * preexistente) y no se crea por accidente una vacia. */
    int rc = sqlite3_open_v2(DB_PARAMS_PATH, &db, SQLITE_OPEN_READWRITE, NULL);

    if (rc != SQLITE_OK) {
        LV_LOG_ERROR("params.db: cannot open %s: %s", DB_PARAMS_PATH,
                     db != NULL ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
        db_close_resources();
        return false;
    }

    if (!db_quick_check(db, why, sizeof why)) {
        LV_LOG_ERROR("params.db: invalid database: %s", why);
        db_close_resources();
        return false;
    }
    if (!db_tables_present(db, DB_BASE_TABLES, DB_ARRAY_COUNT(DB_BASE_TABLES),
                           DB_PARAMS_PATH, why, sizeof why)) {
        LV_LOG_ERROR("params.db: incomplete schema: %s", why);
        db_close_resources();
        return false;
    }

    if (migrations_apply() != 0) {
        LV_LOG_ERROR("params.db: migrations failed");
        db_close_resources();
        return false;
    }

    if (!db_tables_present(db, DB_REQUIRED_TABLES, DB_ARRAY_COUNT(DB_REQUIRED_TABLES),
                           DB_PARAMS_PATH, why, sizeof why)) {
        LV_LOG_ERROR("params.db: incomplete schema after migrations: %s", why);
        db_close_resources();
        return false;
    }

    rc = sqlite3_prepare_v2(db, "INSERT INTO params(name, val) VALUES(?, ?)", -1, &insert_stmt, 0);

    if (rc != SQLITE_OK) {
        LV_LOG_ERROR("Can't prepare insert statement for params: %s", sqlite3_errmsg(db));
        db_close_resources();
        return false;
    }
    return true;
}

bool sql_query_exec(const char *sql) {
    char    *err = NULL;
    int     rc;

    rc = sqlite3_exec(db, sql, NULL, NULL, &err);

    if (rc != SQLITE_OK) {
        LV_LOG_ERROR("SQL failed (%d) on %s: %s", rc, DB_PARAMS_PATH,
                     err != NULL ? err : sqlite3_errmsg(db));
        sqlite3_free(err);
        return false;
    }
    return true;
}

/* Una escritura fallida CONSERVA `dirty` (el valor sigue solo en memoria: no se
 * afirma que quedo persistido) y se informa una vez por racha de errores: el
 * saver corre cada 100 ms y una base RO o llena falla siempre igual, no puede
 * llenar el log. Un exito posterior rearma el aviso. */
static int write_error = 0;

static void report_write_failure(const char *name, int rc) {
    if (rc == write_error) {
        return;
    }
    write_error = rc;
    LV_LOG_ERROR("Persistence write failed: name=%s rc=%d path=%s (%s)", name, rc, DB_PARAMS_PATH,
                 db != NULL ? sqlite3_errmsg(db) : "database is not open");
}

/* sqlite3_step() tiene que dar SQLITE_DONE: cualquier otra cosa (base llena,
 * RO, sentencia no preparada) es una escritura que NO se persistio. */
static bool params_write_step(const char *name, sqlite3_stmt *stmt) {
    int rc = sqlite3_step(stmt);

    if (rc != SQLITE_DONE) {
        report_write_failure(name, rc);
    } else {
        write_error = 0;
    }
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    return rc == SQLITE_DONE;
}

void params_write_int(const char *name, int data, bool *dirty) {
    int rc;

    if (insert_stmt == NULL) {
        report_write_failure(name, SQLITE_MISUSE);
        return;
    }
    rc = sqlite3_bind_text(insert_stmt, 1, name, strlen(name), 0);
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int(insert_stmt, 2, data);
    }
    if (rc != SQLITE_OK) {
        /* Un bind fallido dejaria el INSERT con el valor viejo o NULL: se reporta
         * igual que un step fallido y el dato queda solo en memoria. */
        report_write_failure(name, rc);
        sqlite3_reset(insert_stmt);
        sqlite3_clear_bindings(insert_stmt);
        return;
    }
    if (params_write_step(name, insert_stmt)) {
        *dirty = false;
    }
}

void params_write_int64(const char *name, uint64_t data, bool *dirty) {
    int rc;

    if (insert_stmt == NULL) {
        report_write_failure(name, SQLITE_MISUSE);
        return;
    }
    rc = sqlite3_bind_text(insert_stmt, 1, name, strlen(name), 0);
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_int64(insert_stmt, 2, data);
    }
    if (rc != SQLITE_OK) {
        /* Un bind fallido dejaria el INSERT con el valor viejo o NULL: se reporta
         * igual que un step fallido y el dato queda solo en memoria. */
        report_write_failure(name, rc);
        sqlite3_reset(insert_stmt);
        sqlite3_clear_bindings(insert_stmt);
        return;
    }
    if (params_write_step(name, insert_stmt)) {
        *dirty = false;
    }
}

void params_write_float(const char *name, float data, bool *dirty) {
    int rc;

    if (insert_stmt == NULL) {
        report_write_failure(name, SQLITE_MISUSE);
        return;
    }
    rc = sqlite3_bind_text(insert_stmt, 1, name, strlen(name), 0);
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_double(insert_stmt, 2, (double) data);
    }
    if (rc != SQLITE_OK) {
        /* Un bind fallido dejaria el INSERT con el valor viejo o NULL: se reporta
         * igual que un step fallido y el dato queda solo en memoria. */
        report_write_failure(name, rc);
        sqlite3_reset(insert_stmt);
        sqlite3_clear_bindings(insert_stmt);
        return;
    }
    if (params_write_step(name, insert_stmt)) {
        *dirty = false;
    }
}

void params_write_text(const char *name, const char *data, bool *dirty) {
    int rc;

    if (insert_stmt == NULL) {
        report_write_failure(name, SQLITE_MISUSE);
        return;
    }
    rc = sqlite3_bind_text(insert_stmt, 1, name, strlen(name), 0);
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(insert_stmt, 2, data, strlen(data), 0);
    }
    if (rc != SQLITE_OK) {
        /* Un bind fallido dejaria el INSERT con el valor viejo o NULL: se reporta
         * igual que un step fallido y el dato queda solo en memoria. */
        report_write_failure(name, rc);
        sqlite3_reset(insert_stmt);
        sqlite3_clear_bindings(insert_stmt);
        return;
    }
    if (params_write_step(name, insert_stmt)) {
        *dirty = false;
    }
}

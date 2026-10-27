/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - configuracion de la subida de QSO a logs online.
 */
#define _POSIX_C_SOURCE 200809L

#include "upload_conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_PATH "/mnt/upload.conf"

const char *upload_conf_path(void) {
    const char *env = getenv("UPLOAD_CONF");

    return (env && env[0]) ? env : DEFAULT_PATH;
}

static void defaults(upload_conf_t *conf) {
    memset(conf, 0, sizeof(*conf));
    conf->enabled          = true;
    conf->interval         = 30;
    conf->eqsl_enabled     = true;
    conf->clublog_enabled  = true;
}

static bool parse_bool(const char *v) {
    return !strcmp(v, "1") || !strcmp(v, "true") || !strcmp(v, "yes") ||
           !strcmp(v, "on") || !strcmp(v, "si") || !strcmp(v, "sí");
}

static void copy_value(char *dst, size_t size, const char *src) {
    snprintf(dst, size, "%s", src);
    dst[strcspn(dst, "\r\n")] = '\0';
}

static void parse_line(upload_conf_t *conf, const char *key, const char *value) {
    if (!strcmp(key, "enabled"))               conf->enabled = parse_bool(value);
    else if (!strcmp(key, "interval"))         conf->interval = atoi(value);
    else if (!strcmp(key, "qrz_enabled"))      conf->qrz_enabled = parse_bool(value);
    else if (!strcmp(key, "qrz_api_key"))      copy_value(conf->qrz_api_key, sizeof(conf->qrz_api_key), value);
    else if (!strcmp(key, "eqsl_enabled"))     conf->eqsl_enabled = parse_bool(value);
    else if (!strcmp(key, "eqsl_user"))        copy_value(conf->eqsl_user, sizeof(conf->eqsl_user), value);
    else if (!strcmp(key, "eqsl_pswd"))        copy_value(conf->eqsl_pswd, sizeof(conf->eqsl_pswd), value);
    else if (!strcmp(key, "clublog_enabled"))  conf->clublog_enabled = parse_bool(value);
    else if (!strcmp(key, "clublog_callsign")) copy_value(conf->clublog_callsign, sizeof(conf->clublog_callsign), value);
    else if (!strcmp(key, "clublog_email"))    copy_value(conf->clublog_email, sizeof(conf->clublog_email), value);
    else if (!strcmp(key, "clublog_password")) copy_value(conf->clublog_password, sizeof(conf->clublog_password), value);
    else if (!strcmp(key, "clublog_api"))      copy_value(conf->clublog_api, sizeof(conf->clublog_api), value);
}

bool upload_conf_load(upload_conf_t *conf) {
    if (conf == NULL) {
        return false;
    }

    defaults(conf);

    FILE *f = fopen(upload_conf_path(), "r");

    if (!f) {
        return false;
    }

    char line[320];

    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n");

        if (nl) {
            *nl = '\0';
        }
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        const char *eq = strchr(line, '=');

        if (!eq) {
            continue;
        }

        char key[64];
        snprintf(key, sizeof(key), "%.*s", (int) (eq - line), line);

        /* Los valores pueden venir con espacios o comillas; se limpian. */
        const char *value = eq + 1;

        while (*value == ' ' || *value == '\t') {
            value++;
        }

        parse_line(conf, key, value);
    }

    fclose(f);

    if (conf->interval < 10) {
        conf->interval = 30;
    }

    return true;
}

bool upload_conf_save(const upload_conf_t *conf) {
    const char *path = upload_conf_path();
    char        tmp[112];

    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE *f = fopen(tmp, "w");

    if (!f) {
        return false;
    }

    fprintf(f,
            "# X6200 — subida automatica de QSO a logs online (lo edita Ajustes > Log uploads)\n"
            "enabled=%d\n"
            "interval=%d\n"
            "qrz_enabled=%d\n"
            "qrz_api_key=%s\n"
            "eqsl_enabled=%d\n"
            "eqsl_user=%s\n"
            "eqsl_pswd=%s\n"
            "clublog_enabled=%d\n"
            "clublog_callsign=%s\n"
            "clublog_email=%s\n"
            "clublog_password=%s\n"
            "clublog_api=%s\n",
            conf->enabled ? 1 : 0,
            conf->interval,
            conf->qrz_enabled ? 1 : 0, conf->qrz_api_key,
            conf->eqsl_enabled ? 1 : 0, conf->eqsl_user, conf->eqsl_pswd,
            conf->clublog_enabled ? 1 : 0, conf->clublog_callsign, conf->clublog_email,
            conf->clublog_password, conf->clublog_api);

    if (fclose(f) != 0) {
        unlink(tmp);
        return false;
    }

    chmod(tmp, 0600);

    return rename(tmp, path) == 0;
}

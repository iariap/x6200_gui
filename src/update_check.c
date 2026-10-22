/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - new release alert (see update_check.h).
 */

#include "update_check.h"

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CONF_PATH      "/mnt/update_check.conf"
#define DEFAULT_SOURCE "https://api.github.com/repos/iariap/x6200_gui/releases/latest"

#define BODY_MAX       4096
#define CMD_MAX        1024

#define RETRIES        20           /* up to ~20 minutes waiting for the WiFi */
#define FIRST_WAIT_S   30

static const char *conf_path(void) {
    const char *env = getenv("UPDATE_CHECK_CONF");

    return (env && env[0]) ? env : CONF_PATH;
}

/* --- configuration ---------------------------------------------------- */

static void conf_defaults(bool *enabled, char *source, size_t source_sz) {
    *enabled = true;
    snprintf(source, source_sz, "%s", DEFAULT_SOURCE);
}

static void conf_read(bool *enabled, char *source, size_t source_sz) {
    conf_defaults(enabled, source, source_sz);

    FILE *f = fopen(conf_path(), "r");

    if (!f) {
        return;
    }

    char line[512];

    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n");

        if (nl) {
            *nl = '\0';
        }

        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        char *eq = strchr(line, '=');

        if (!eq) {
            continue;
        }

        *eq = '\0';

        const char *key = line;
        const char *val = eq + 1;

        if (strcmp(key, "enabled") == 0) {
            *enabled = (atoi(val) != 0);
        } else if (strcmp(key, "source") == 0 && val[0]) {
            snprintf(source, source_sz, "%s", val);
        }
    }

    fclose(f);
}

/* --- version helpers -------------------------------------------------- */

bool update_newer(const char *a, const char *b) {
    if (!a || !b || !*a || !*b) {
        return false;               /* nothing to compare against */
    }

    a += (*a == 'v');
    b += (*b == 'v');

    if (!*a || !*b) {
        return false;
    }

    while (*a || *b) {
        char *ea, *eb;
        long  x = strtol(a, &ea, 10);
        long  y = strtol(b, &eb, 10);

        if (x != y) {
            return x > y;
        }

        a = ea;
        b = eb;

        if (*a == '.') {
            a++;
        }

        if (*b == '.') {
            b++;
        }

        /* Stop at anything that is not a number: "v1.2-beta" == "v1.2" */
        if ((*a && !isdigit((unsigned char) *a)) || (*b && !isdigit((unsigned char) *b))) {
            break;
        }
    }

    return false;
}

static void trim(char *s) {
    size_t n = strlen(s);

    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }

    size_t i = 0;

    while (s[i] == ' ' || s[i] == '\t') {
        i++;
    }

    if (i) {
        memmove(s, s + i, strlen(s + i) + 1);
    }
}

bool update_check_parse_tag(const char *body, char *out, size_t out_sz) {
    if (!body || !out || out_sz < 2) {
        return false;
    }

    out[0] = '\0';

    const char *key = strstr(body, "\"tag_name\"");

    if (key) {
        const char *p = strchr(key + 10, ':');

        if (p) {
            p = strchr(p, '"');

            if (p) {
                const char *e = strchr(++p, '"');

                if (e && (size_t)(e - p) < out_sz) {
                    memcpy(out, p, (size_t)(e - p));
                    out[e - p] = '\0';
                } else {
                    return false;
                }
            }
        }
    } else {
        /* A bare version line, e.g. the output of `python -c "...print(tag)"`. */
        snprintf(out, out_sz, "%s", body);
        trim(out);

        if (out[0] != 'v' && !isdigit((unsigned char) out[0])) {
            out[0] = '\0';
            return false;
        }
    }

    if (!out[0]) {
        return false;
    }

    /* Keep only the leading version-looking token ("v0.1.2", "0.1.2"). */
    for (char *p = out; *p; p++) {
        if (!isdigit((unsigned char) *p) && *p != '.' && *p != 'v' && *p != 'V') {
            *p = '\0';
            break;
        }
    }

    return out[0] != '\0';
}

/* --- fetching --------------------------------------------------------- */

static bool fetch_body(char *out, size_t out_sz) {
    char cmd[CMD_MAX];
    const char *override = getenv("UPDATE_CHECK_CMD");
    bool  enabled;
    char  source[512];

    conf_read(&enabled, source, sizeof(source));

    if (!enabled) {
        return false;
    }

    if (override && override[0]) {
        snprintf(cmd, sizeof(cmd), "%s", override);
    } else {
        if (strchr(source, '\'')) {          /* refuse to build a broken command */
            return false;
        }

        snprintf(cmd, sizeof(cmd),
                 "python3 -c \"import urllib.request as u;"
                 "print(u.urlopen('%s',timeout=15).read().decode())\" 2>/dev/null",
                 source);
    }

    FILE *p = popen(cmd, "r");

    if (!p) {
        return false;
    }

    out[0] = '\0';

    if (!fgets(out, (int) out_sz, p)) {
        out[0] = '\0';
    }

    pclose(p);

    return out[0] != '\0';
}

bool update_check_run_once(const char *current, char *newer_out, size_t out_sz) {
    char body[BODY_MAX];
    char tag[64];

    if (newer_out && out_sz) {
        newer_out[0] = '\0';
    }

    if (!current || !fetch_body(body, sizeof(body))) {
        return false;
    }

    if (!update_check_parse_tag(body, tag, sizeof(tag))) {
        return false;
    }

    if (!update_newer(tag, current)) {
        return false;
    }

    if (newer_out && out_sz) {
        snprintf(newer_out, out_sz, "%s", tag);
    }

    return true;
}

/* Fetch and parse only: true when the source answered with a usable tag,
 * even if it is not newer than ours. */
static bool fetch_tag(char *tag, size_t tag_sz) {
    char body[BODY_MAX];

    if (!fetch_body(body, sizeof(body))) {
        return false;
    }

    return update_check_parse_tag(body, tag, tag_sz);
}

/* --- startup thread --------------------------------------------------- */

static const char *g_current;
static void (*g_notify)(const char *version);

static void *check_thread(void *arg) {
    (void) arg;

    for (int i = 0; i < RETRIES; i++) {
        char tag[64];

        sleep(i ? 60 : FIRST_WAIT_S);

        if (!fetch_tag(tag, sizeof(tag))) {
            continue;               /* no network yet: keep waiting */
        }

        /* The source answered: we know whether there is something new. */
        if (update_newer(tag, g_current)) {
            g_notify(tag);
        }

        break;
    }

    return NULL;
}

void update_check_start(const char *current, void (*notify)(const char *version)) {
    pthread_t t;
    bool      enabled;
    char      source[512];

    if (!current || !notify) {
        return;
    }

    conf_read(&enabled, source, sizeof(source));

    if (!enabled) {
        return;
    }

    g_current = current;
    g_notify  = notify;

    if (pthread_create(&t, NULL, check_thread, NULL) == 0) {
        pthread_detach(t);
    }
}

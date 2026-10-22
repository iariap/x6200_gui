/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Persistent "last breath" record (issue #121). See lastbreath.h.
 */

#include "lastbreath.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* The record is one line. Keep the stack buffers explicit: the write path must
 * not allocate, and it runs from the GUI loop (and from atexit). */
#define LASTBREATH_RECORD_MAX 512

static void sanitize(char *dst, size_t n, const char *src) {
    if (n == 0) {
        return;
    }

    if (src == NULL) {
        src = "none";
    }

    size_t i = 0;

    for (; src[i] != '\0' && i + 1 < n; i++) {
        unsigned char c = (unsigned char) src[i];

        /* One record per line: a message with a newline must not turn into a
         * second (unparseable) line. */
        dst[i] = (c < 0x20 || c == 0x7f) ? ' ' : (char) c;
    }

    dst[i] = '\0';
}

void lastbreath_state_init(lastbreath_state_t *s) {
    if (s == NULL) {
        return;
    }

    memset(s, 0, sizeof(*s));
    sanitize(s->boot, sizeof(s->boot), "none");
    sanitize(s->radio, sizeof(s->radio), "none");
    sanitize(s->app, sizeof(s->app), "none");
    sanitize(s->last_event, sizeof(s->last_event), "none");
    sanitize(s->msg, sizeof(s->msg), "none");
}

size_t lastbreath_format(char *buf, size_t n, const lastbreath_state_t *s) {
    if (buf == NULL || n == 0) {
        return 0;
    }

    char boot[LASTBREATH_BOOT_MAX];
    char radio[16];
    char app[24];
    char last_event[16];
    char msg[LASTBREATH_TEXT_MAX];
    char line[LASTBREATH_RECORD_MAX];

    sanitize(boot, sizeof(boot), s != NULL ? s->boot : NULL);
    sanitize(radio, sizeof(radio), s != NULL ? s->radio : NULL);
    sanitize(app, sizeof(app), s != NULL ? s->app : NULL);
    sanitize(last_event, sizeof(last_event), s != NULL ? s->last_event : NULL);
    sanitize(msg, sizeof(msg), s != NULL ? s->msg : NULL);

    unsigned long long uptime_ms = s != NULL ? (unsigned long long) s->uptime_ms : 0ULL;
    int    clean      = (s != NULL && s->clean) ? 1 : 0;
    int    tx         = (s != NULL && s->tx) ? 1 : 0;
    unsigned int ev_pending = s != NULL ? s->ev_pending : 0;
    unsigned int ev_peak    = s != NULL ? s->ev_peak : 0;
    unsigned int ev_dropped = s != NULL ? s->ev_dropped : 0;
    unsigned int prev_us    = s != NULL ? s->prev_write_us : 0;

    int len = snprintf(line, sizeof(line),
                       "lastbreath v=1 up_s=%llu up_ms=%llu clean=%d boot=%s radio=%s "
                       "tx=%d app=%s ",
                       uptime_ms / 1000ULL, uptime_ms, clean, boot, radio, tx, app);

    if (len < 0 || (size_t) len >= sizeof(line)) {
        buf[0] = '\0';
        return 0;
    }

    if (s != NULL && s->bat_valid) {
        int n2 = snprintf(line + len, sizeof(line) - (size_t) len,
                          "bat_mv=%ld ext_mv=%ld cap=%d chg=%d ",
                          (long) s->bat_mv, (long) s->ext_mv, s->bat_pct,
                          s->charging ? 1 : 0);

        if (n2 < 0 || (size_t) n2 >= sizeof(line) - (size_t) len) {
            buf[0] = '\0';
            return 0;
        }
        len += n2;
    } else {
        int n2 = snprintf(line + len, sizeof(line) - (size_t) len, "bat=na ");

        if (n2 < 0 || (size_t) n2 >= sizeof(line) - (size_t) len) {
            buf[0] = '\0';
            return 0;
        }
        len += n2;
    }

    int n3 = snprintf(line + len, sizeof(line) - (size_t) len,
                      "ev=%u/%u/%u last=%s wus=%u msg=%s\n",
                      ev_pending, ev_peak, ev_dropped, last_event, prev_us, msg);

    if (n3 < 0 || (size_t) n3 >= sizeof(line) - (size_t) len) {
        buf[0] = '\0';
        return 0;
    }
    len += n3;

    size_t copy = (size_t) len < n ? (size_t) len : n - 1;

    memcpy(buf, line, copy);
    buf[copy] = '\0';

    return copy;
}

static int write_all(int fd, const char *data, size_t len) {
    size_t done = 0;

    while (done < len) {
        ssize_t rc = write(fd, data + done, len - done);

        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (rc == 0) {
            return -1;
        }
        done += (size_t) rc;
    }

    return 0;
}

int lastbreath_write_atomic(const char *path, const char *data, size_t len) {
    if (path == NULL || data == NULL) {
        return -1;
    }

    size_t path_len = strlen(path);

    if (path_len == 0 || path_len + 5 > LASTBREATH_PATH_MAX) {
        return -1;
    }

    char tmp[LASTBREATH_PATH_MAX];

    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        return -1;
    }

    if (write_all(fd, data, len) != 0) {
        close(fd);
        unlink(tmp);
        return -1;
    }

    /* Make the DATA bytes durable before the rename publishes them. The record
     * is small and the interval is seconds, so this is cheap (see evidence). */
    (void) fdatasync(fd);

    if (close(fd) != 0) {
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }

    return 0;
}

static int read_previous(const char *path, char *out, size_t n) {
    if (path == NULL || out == NULL || n < 2) {
        return -1;
    }

    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        return -1;
    }

    size_t done = 0;

    while (done + 1 < n) {
        ssize_t rc = read(fd, out + done, n - 1 - done);

        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        if (rc == 0) {
            break;
        }
        done += (size_t) rc;
    }

    close(fd);
    out[done] = '\0';

    return (int) done;
}

static int lastbreath_classify(const char *text) {
    if (text == NULL || text[0] == '\0') {
        return -1;
    }

    const char *p = strstr(text, "clean=");

    if (p == NULL) {
        /* A record without the marker cannot prove a clean shutdown. */
        return 1;
    }

    return p[6] == '1' ? 0 : 1;
}

lastbreath_boot_t lastbreath_boot(const char *path, char *out, size_t n) {
    if (out != NULL && n > 0) {
        out[0] = '\0';
    }

    char previous[LASTBREATH_RECORD_MAX];
    int  got = read_previous(path, previous, sizeof(previous));

    if (got < 0) {
        return errno == ENOENT ? LASTBREATH_BOOT_ABSENT : LASTBREATH_BOOT_ERROR;
    }

    if (got == 0) {
        return LASTBREATH_BOOT_ABSENT;
    }

    if (lastbreath_classify(previous) == 0) {
        return LASTBREATH_BOOT_CLEAN;
    }

    if (out == NULL || n == 0) {
        return LASTBREATH_BOOT_UNCLEAN;
    }

    /* One line, always: prefix + the previous record with control characters
     * turned into spaces. That is what the next boot logs for triage. */
    size_t i = 0;
    const char *prefix = "LASTBREATH ";

    while (prefix[i] != '\0' && i + 1 < n) {
        out[i] = prefix[i];
        i++;
    }

    for (size_t j = 0; previous[j] != '\0' && i + 1 < n; j++) {
        unsigned char c = (unsigned char) previous[j];

        out[i++] = (c < 0x20 || c == 0x7f) ? ' ' : (char) c;
    }

    /* Trim the spaces that came from the trailing newline(s), then add one. */
    while (i > 0 && out[i - 1] == ' ') {
        i--;
    }

    if (i + 1 < n) {
        out[i++] = '\n';
    }

    out[i] = '\0';

    return LASTBREATH_BOOT_UNCLEAN;
}

int lastbreath_mark_clean(const char *path, const lastbreath_state_t *s) {
    lastbreath_state_t copy;

    if (s != NULL) {
        copy = *s;
    } else {
        lastbreath_state_init(&copy);
    }

    copy.clean = true;

    char line[LASTBREATH_LINE_MAX];
    size_t len = lastbreath_format(line, sizeof(line), &copy);

    if (len == 0) {
        return -1;
    }

    return lastbreath_write_atomic(path, line, len);
}

bool lastbreath_read_boot_id(char *out, size_t n) {
    if (out == NULL || n == 0) {
        return false;
    }

    out[0] = '\0';

    FILE *f = fopen("/proc/sys/kernel/random/boot_id", "r");

    if (f == NULL) {
        sanitize(out, n, "none");
        return false;
    }

    char id[64] = {0};

    if (fgets(id, sizeof(id), f) == NULL) {
        fclose(f);
        sanitize(out, n, "none");
        return false;
    }

    fclose(f);

    /* The UUID is 36 chars with hyphens; the first 8 hex chars are enough to
     * tell one boot from another in a one-line record. */
    size_t j = 0;

    for (size_t i = 0; id[i] != '\0' && j + 1 < n && j < 8; i++) {
        char c = id[i];

        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            out[j++] = c;
        }
    }

    out[j] = '\0';

    if (j == 0) {
        sanitize(out, n, "none");
        return false;
    }

    return true;
}

uint64_t lastbreath_read_uptime_ms(void) {
    FILE *f = fopen("/proc/uptime", "r");

    if (f == NULL) {
        return 0;
    }

    unsigned long long seconds = 0;
    unsigned long long centis  = 0;
    int rc = fscanf(f, "%llu.%llu", &seconds, &centis);

    fclose(f);

    if (rc != 2) {
        return 0;
    }

    return seconds * 1000ULL + (centis % 100ULL) * 10ULL;
}

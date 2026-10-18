/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Persistent "last breath" record (issue #121).
 *
 *  Four whole-radio hangs left no evidence: the GUI log lives in tmpfs and is
 *  only archived on a clean exit, so a hard power cut loses it. This module
 *  writes a tiny fixed-format record to the DATA partition every few seconds
 *  (tmp file + rename, so a reader never sees a half-written line). On the next
 *  start the previous record is reported as one `LASTBREATH ...` line unless it
 *  was marked as a clean shutdown.
 *
 *  It is deliberately pure libc: no LVGL, no params, no heap on the write path
 *  (the line is formatted in a stack buffer and written with open/write). If
 *  the path is not there (no DATA partition, bench without /mnt), the write
 *  fails and the caller keeps running.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LASTBREATH_LINE_MAX 384
#define LASTBREATH_PATH_MAX 256
#define LASTBREATH_BOOT_MAX 16
#define LASTBREATH_TEXT_MAX 64

typedef struct {
    uint64_t uptime_ms;        /* /proc/uptime, in milliseconds */
    uint32_t prev_write_us;    /* duration of the previous write (self-measured) */
    char     boot[LASTBREATH_BOOT_MAX];
    char     radio[16];        /* RADIO_RX / RADIO_TX / ... */
    char     app[24];          /* home / SWRScan / FT8 / ... */
    bool     tx;
    bool     bat_valid;        /* false: battery voltage is not published yet */
    int32_t  bat_mv;
    int32_t  ext_mv;
    int      bat_pct;
    bool     charging;
    uint32_t ev_pending;
    uint32_t ev_peak;
    uint32_t ev_dropped;
    char     last_event[16];
    char     msg[LASTBREATH_TEXT_MAX];  /* last message shown by the GUI */
    bool     clean;            /* true only in the record written on a clean exit */
} lastbreath_state_t;

typedef enum {
    LASTBREATH_BOOT_ABSENT = 0,  /* no previous record */
    LASTBREATH_BOOT_CLEAN,       /* previous run marked a clean shutdown */
    LASTBREATH_BOOT_UNCLEAN,     /* previous run did not: out has the LASTBREATH line */
    LASTBREATH_BOOT_ERROR,       /* the path exists but could not be read */
} lastbreath_boot_t;

/* Zero the state and set every text field to "none". */
void lastbreath_state_init(lastbreath_state_t *s);

/* Deterministic one-line format plus a trailing newline. The result is always
 * NUL-terminated; the return value is the number of bytes written (not the
 * would-be length). Never allocates. */
size_t lastbreath_format(char *buf, size_t n, const lastbreath_state_t *s);

/* Write `len` bytes to `path` atomically: `<path>.tmp` + fdatasync + rename.
 * No heap. Returns 0 on success, -1 on any failure (missing directory, a path
 * longer than LASTBREATH_PATH_MAX, I/O error) after removing the tmp file. */
int lastbreath_write_atomic(const char *path, const char *data, size_t len);

/* Read the previous record without modifying it. When the previous record was
 * not a clean shutdown, `out` receives one `LASTBREATH <record>` line (control
 * characters collapsed to spaces, exactly one trailing newline). Returns the
 * classification; `out[0]` is '\0' unless it is UNCLEAN. */
lastbreath_boot_t lastbreath_boot(const char *path, char *out, size_t n);

/* Write the same state with clean=true. Used by the clean exits (atexit and the
 * software power-off path). */
int lastbreath_mark_clean(const char *path, const lastbreath_state_t *s);

/* First 8 hex chars of /proc/sys/kernel/random/boot_id, or "none". */
bool lastbreath_read_boot_id(char *out, size_t n);

/* Milliseconds from /proc/uptime, or 0 when it cannot be read. */
uint64_t lastbreath_read_uptime_ms(void);

#ifdef __cplusplus
}
#endif

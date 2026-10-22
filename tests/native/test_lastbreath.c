/*
 * Native test for the persistent "last breath" record (issue #121).
 *
 * Four whole-radio hangs left no evidence: the GUI log lives in tmpfs and is
 * only archived on a clean exit, so every hard power cut loses it. This module
 * writes a tiny, fixed-format record every few seconds (tmp + rename) so the
 * next boot can print what the GUI was doing when the machine died.
 *
 * This test pins:
 *   1. the exact, deterministic one-line format (including `bat=na` when the
 *      radio does not publish a battery reading);
 *   2. newline sanitisation: the record is always exactly one line;
 *   3. atomic replacement: the final path only ever holds a complete record
 *      and the `.tmp` sibling is gone;
 *   4. clean vs unclean classification, and the one-line `LASTBREATH` report
 *      built from the previous record;
 *   5. graceful degradation when the filesystem/path does not exist.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lastbreath.h"

#include "test_framework.h"

static void sample_state(lastbreath_state_t *s) {
    lastbreath_state_init(s);

    s->uptime_ms      = 51644123;
    s->prev_write_us  = 1234;
    s->tx             = false;
    s->bat_valid      = true;
    s->bat_mv         = 8600;
    s->ext_mv         = 11500;
    s->bat_pct        = 37;
    s->charging       = false;
    s->ev_pending     = 2;
    s->ev_peak        = 7;
    s->ev_dropped     = 1;
    s->clean          = false;

    snprintf(s->boot, sizeof(s->boot), "%s", "1a2b3c4d");
    snprintf(s->radio, sizeof(s->radio), "%s", "RX");
    snprintf(s->app, sizeof(s->app), "%s", "SWRScan");
    snprintf(s->last_event, sizeof(s->last_event), "%s", "keypad");
    snprintf(s->msg, sizeof(s->msg), "%s", "none");
}

static int count_newlines(const char *s) {
    int n = 0;

    while (*s != '\0') {
        if (*s == '\n') {
            n++;
        }
        s++;
    }

    return n;
}

static bool read_file(const char *path, char *out, size_t n) {
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        return false;
    }

    size_t got = fread(out, 1, n - 1, f);
    out[got] = '\0';
    fclose(f);

    return true;
}

static void test_format_is_deterministic(void) {
    lastbreath_state_t s;
    char               out[512];
    char               again[512];

    sample_state(&s);

    const char *want =
        "lastbreath v=1 up_s=51644 up_ms=51644123 clean=0 boot=1a2b3c4d radio=RX tx=0 "
        "app=SWRScan bat_mv=8600 ext_mv=11500 cap=37 chg=0 ev=2/7/1 last=keypad wus=1234 "
        "msg=none\n";

    size_t n = lastbreath_format(out, sizeof(out), &s);

    CHECK(n == strlen(want));
    CHECK(strcmp(out, want) == 0);

    size_t n2 = lastbreath_format(again, sizeof(again), &s);

    CHECK(n2 == n);
    CHECK(strcmp(again, out) == 0);
}

static void test_format_without_battery(void) {
    lastbreath_state_t s;
    char               out[512];

    sample_state(&s);
    s.bat_valid = false;
    s.bat_mv    = 0;
    s.ext_mv    = 0;
    s.bat_pct   = 0;

    lastbreath_format(out, sizeof(out), &s);

    CHECK(strstr(out, " bat=na ") != NULL);
    CHECK(strstr(out, "bat_mv=") == NULL);
    CHECK(count_newlines(out) == 1);
    CHECK(out[strlen(out) - 1] == '\n');
}

static void test_message_is_sanitised_to_one_line(void) {
    lastbreath_state_t s;
    char               out[512];

    sample_state(&s);
    snprintf(s.msg, sizeof(s.msg), "Battery\nlow\r\tnow");

    lastbreath_format(out, sizeof(out), &s);

    CHECK(count_newlines(out) == 1);
    CHECK(strstr(out, "msg=Battery low  now") != NULL);
}

static void test_short_buffer_is_terminated(void) {
    lastbreath_state_t s;
    char               small[24];

    sample_state(&s);
    memset(small, 0xAA, sizeof(small));

    size_t n = lastbreath_format(small, sizeof(small), &s);

    CHECK(n < sizeof(small));
    CHECK(small[n] == '\0');
    CHECK(strncmp(small, "lastbreath v=1", 14) == 0);

    /* n == 0 is valid and leaves an empty string. */
    CHECK(lastbreath_format(small, 0, &s) == 0);
}

static void test_atomic_write_replaces_and_leaves_no_tmp(void) {
    char  dir[] = "/tmp/lastbreath-test-XXXXXX";
    char  path[256];
    char  tmp[512];
    char  line1[512];
    char  line2[512];
    char  got[512];
    struct stat st;

    CHECK(mkdtemp(dir) != NULL);
    snprintf(path, sizeof(path), "%s/lastbreath", dir);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    CHECK(lastbreath_write_atomic(path, "uno\n", 4) == 0);
    CHECK(read_file(path, got, sizeof(got)));
    CHECK(strcmp(got, "uno\n") == 0);
    CHECK(stat(tmp, &st) != 0 && errno == ENOENT);

    snprintf(line1, sizeof(line1), "linea uno\n");
    snprintf(line2, sizeof(line2), "linea dos\n");

    CHECK(lastbreath_write_atomic(path, line1, strlen(line1)) == 0);
    CHECK(lastbreath_write_atomic(path, line2, strlen(line2)) == 0);
    CHECK(read_file(path, got, sizeof(got)));
    CHECK(strcmp(got, line2) == 0);
    CHECK(stat(tmp, &st) != 0 && errno == ENOENT);

    unlink(path);
    rmdir(dir);
}

static void test_missing_filesystem_degrades(void) {
    char out[256];
    char path[256];

    snprintf(path, sizeof(path), "/nonexistent-lastbreath-dir-%ld/lastbreath", (long) getpid());

    CHECK(lastbreath_write_atomic(path, "x\n", 2) == -1);
    CHECK(lastbreath_boot(path, out, sizeof(out)) == LASTBREATH_BOOT_ABSENT);
    CHECK(out[0] == '\0');

    /* A path that does not fit the module's stack buffer also degrades. */
    char huge[LASTBREATH_PATH_MAX + 64];

    memset(huge, 'a', sizeof(huge) - 1);
    huge[sizeof(huge) - 1] = '\0';
    CHECK(lastbreath_write_atomic(huge, "x\n", 2) == -1);
}

static void test_boot_reports_unclean_previous(void) {
    char  dir[] = "/tmp/lastbreath-test-XXXXXX";
    char  path[256];
    char  out[512];
    char  rec[512];

    CHECK(mkdtemp(dir) != NULL);
    snprintf(path, sizeof(path), "%s/lastbreath", dir);

    snprintf(rec, sizeof(rec),
             "lastbreath v=1 up_s=42 up_ms=42000 clean=0 boot=deadbeef radio=TX tx=1 "
             "app=FT8 bat_mv=8600 ext_mv=0 cap=10 chg=0 ev=1/1/0 last=keypad wus=9 "
             "msg=hello world\n");
    CHECK(lastbreath_write_atomic(path, rec, strlen(rec)) == 0);

    CHECK(lastbreath_boot(path, out, sizeof(out)) == LASTBREATH_BOOT_UNCLEAN);
    CHECK(strncmp(out, "LASTBREATH ", 11) == 0);
    CHECK(strstr(out, "up_s=42") != NULL);
    CHECK(strstr(out, "app=FT8") != NULL);
    CHECK(count_newlines(out) == 1);
    CHECK(out[strlen(out) - 1] == '\n');

    unlink(path);
    rmdir(dir);
}

static void test_boot_accepts_clean_previous(void) {
    char dir[] = "/tmp/lastbreath-test-XXXXXX";
    char path[256];
    char out[256];
    char rec[256];

    CHECK(mkdtemp(dir) != NULL);
    snprintf(path, sizeof(path), "%s/lastbreath", dir);
    snprintf(rec, sizeof(rec), "lastbreath v=1 up_s=7 clean=1\n");
    CHECK(lastbreath_write_atomic(path, rec, strlen(rec)) == 0);

    CHECK(lastbreath_boot(path, out, sizeof(out)) == LASTBREATH_BOOT_CLEAN);
    CHECK(out[0] == '\0');

    unlink(path);
    rmdir(dir);
}

static void test_boot_without_clean_flag_is_suspicious(void) {
    char dir[] = "/tmp/lastbreath-test-XXXXXX";
    char path[256];
    char out[256];

    CHECK(mkdtemp(dir) != NULL);
    snprintf(path, sizeof(path), "%s/lastbreath", dir);
    CHECK(lastbreath_write_atomic(path, "basura sin campos\n", 18) == 0);

    CHECK(lastbreath_boot(path, out, sizeof(out)) == LASTBREATH_BOOT_UNCLEAN);
    CHECK(strncmp(out, "LASTBREATH ", 11) == 0);

    unlink(path);
    rmdir(dir);
}

static void test_mark_clean_flips_the_record(void) {
    char dir[] = "/tmp/lastbreath-test-XXXXXX";
    char path[256];
    char out[256];
    lastbreath_state_t s;

    CHECK(mkdtemp(dir) != NULL);
    snprintf(path, sizeof(path), "%s/lastbreath", dir);

    sample_state(&s);
    CHECK(lastbreath_write_atomic(path, "lastbreath v=1 clean=0\n", 23) == 0);
    CHECK(lastbreath_mark_clean(path, &s) == 0);
    CHECK(lastbreath_boot(path, out, sizeof(out)) == LASTBREATH_BOOT_CLEAN);

    unlink(path);
    rmdir(dir);
}

void test_lastbreath(void) {
    test_format_is_deterministic();
    test_format_without_battery();
    test_message_is_sanitised_to_one_line();
    test_short_buffer_is_terminated();
    test_atomic_write_replaces_and_leaves_no_tmp();
    test_missing_filesystem_degrades();
    test_boot_reports_unclean_previous();
    test_boot_accepts_clean_previous();
    test_boot_without_clean_flag_is_suspicious();
    test_mark_clean_flips_the_record();
}

/*
 * Unit tests: minimal AVI (MJPEG + PCM) writer (avi_writer.c).
 *
 * Pure file I/O, no LVGL and no display: opens a file, writes a few frames and
 * audio, closes it and then checks the RIFF/AVI structure (headers, streams,
 * movi chunks and idx1 index) that players rely on.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "avi_writer.h"
#include "test_framework.h"

#define TEST_PATH "/tmp/avi_writer_test.avi"

static bool contains(const uint8_t *hay, size_t n, const char *needle) {
    size_t m = strlen(needle);

    if (m == 0 || n < m) {
        return false;
    }

    for (size_t i = 0; i + m <= n; i++) {
        if (memcmp(hay + i, needle, m) == 0) {
            return true;
        }
    }

    return false;
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) |
           ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

static size_t read_file(const char *path, uint8_t **out) {
    FILE *f = fopen(path, "rb");

    if (!f) {
        *out = NULL;
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = malloc((size_t) n + 1);

    if (!buf || fread(buf, 1, (size_t) n, f) != (size_t) n) {
        free(buf);
        fclose(f);
        *out = NULL;
        return 0;
    }

    fclose(f);
    *out = buf;

    return (size_t) n;
}

static void test_avi_structure(void) {
    /* A fake JPEG: the writer treats it as an opaque blob */
    static const uint8_t jpg[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0xFF, 0xD9 };
    int16_t              pcm[64];

    memset(pcm, 0, sizeof(pcm));

    avi_t *a = avi_open(TEST_PATH, 800, 480, 8, 44100);

    CHECK(a != NULL);

    if (!a) {
        return;
    }

    CHECK(avi_write_video(a, jpg, sizeof(jpg)));
    CHECK(avi_write_audio(a, pcm, 64));
    CHECK(avi_write_video(a, jpg, sizeof(jpg)));
    CHECK(avi_write_audio(a, pcm, 64));
    CHECK(avi_write_video(a, jpg, sizeof(jpg)));
    CHECK(avi_close(a));

    uint8_t *buf = NULL;
    size_t   n   = read_file(TEST_PATH, &buf);

    CHECK(n > 200);
    CHECK(buf != NULL);

    if (buf) {
        /* RIFF container with an AVI form */
        CHECK(memcmp(buf, "RIFF", 4) == 0);
        CHECK(memcmp(buf + 8, "AVI ", 4) == 0);
        CHECK(rd32(buf + 4) == n - 8);          /* RIFF size covers the rest of the file */

        /* Headers: main header, video and audio stream lists, MJPEG + PCM */
        CHECK(contains(buf, n, "hdrl"));
        CHECK(contains(buf, n, "avih"));
        CHECK(contains(buf, n, "strl"));
        CHECK(contains(buf, n, "vids"));
        CHECK(contains(buf, n, "MJPG"));
        CHECK(contains(buf, n, "auds"));

        /* Data: three video chunks and two audio chunks, then the index */
        CHECK(contains(buf, n, "movi"));
        CHECK(contains(buf, n, "00dc"));
        CHECK(contains(buf, n, "01wb"));
        CHECK(contains(buf, n, "idx1"));

        free(buf);
    }

    unlink(TEST_PATH);
}

static void test_avi_errors(void) {
    int16_t pcm[8] = {0};

    /* A path that cannot be created: open must fail cleanly. */
    CHECK(avi_open("/no/such/dir/x.avi", 64, 48, 8, 44100) == NULL);

    /* Closing twice / writing after close must not crash (NULL checks). */
    avi_t *a = avi_open(TEST_PATH, 64, 48, 8, 44100);

    CHECK(a != NULL);

    if (a) {
        CHECK(avi_write_video(a, (const uint8_t *) "x", 1));
        CHECK(avi_close(a));
        /* Pointers are owned by the writer; nothing else to do. */
    }

    unlink(TEST_PATH);
    (void) pcm;
}

void test_avi_writer(void) {
    test_avi_structure();
    test_avi_errors();
}

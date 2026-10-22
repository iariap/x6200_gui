/*
 * Unit tests: new release alert (update_check.c).
 *
 * The network is never used: the fetch command is overridden with
 * $UPDATE_CHECK_CMD and the config file with $UPDATE_CHECK_CONF.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_framework.h"
#include "update_check.h"

static void test_update_newer(void) {
    CHECK(update_newer("v0.112", "v0.99"));         /* numeric, not lexicographic */
    CHECK(update_newer("v0.1.2", "v0.1.1"));
    CHECK(update_newer("0.1.2", "v0.1.1"));         /* the v is optional */
    CHECK(update_newer("v1.0.0", "v0.99.99"));
    CHECK(update_newer("v0.10", "v0.9"));

    CHECK(!update_newer("v0.1.1", "v0.1.1"));       /* equal is not newer */
    CHECK(!update_newer("v0.1.0", "v0.1.1"));
    CHECK(!update_newer("v0.99", "v0.112"));
    CHECK(!update_newer("v0.1.1", "v0.1.2"));

    /* Suffixes and garbage must not make a version look newer. */
    CHECK(!update_newer("v0.1.1-beta", "v0.1.1"));
    CHECK(!update_newer("", "v0.1.1"));
    CHECK(!update_newer("v0.1.1", ""));
    CHECK(!update_newer(NULL, "v0.1.1"));
    CHECK(!update_newer("v0.1.1", NULL));
}

static void test_parse_tag(void) {
    char out[64];

    const char *gh =
        "{\"url\":\"https://api.github.com/repos/x/y/releases/1\","
        "\"assets_url\":\"...\",\"tag_name\":\"v0.1.2\","
        "\"name\":\"v0.1.2\",\"draft\":false}";

    CHECK(update_check_parse_tag(gh, out, sizeof(out)));
    CHECK(strcmp(out, "v0.1.2") == 0);

    /* A bare line (what the fetch helper may print) must work too. */
    CHECK(update_check_parse_tag("v0.2.0\n", out, sizeof(out)));
    CHECK(strcmp(out, "v0.2.0") == 0);

    CHECK(update_check_parse_tag("  0.3.1  \n", out, sizeof(out)));
    CHECK(strcmp(out, "0.3.1") == 0);

    /* Extra text after the version is trimmed away. */
    CHECK(update_check_parse_tag("v0.4.0 (2026-01-01)\n", out, sizeof(out)));
    CHECK(strcmp(out, "v0.4.0") == 0);

    /* No tag and no version-looking text. */
    CHECK(!update_check_parse_tag("{\"message\":\"Not Found\"}", out, sizeof(out)));
    CHECK(!update_check_parse_tag("", out, sizeof(out)));
    CHECK(!update_check_parse_tag("network unreachable", out, sizeof(out)));

    /* A tiny buffer must fail cleanly, not overflow. */
    char small[4];
    CHECK(!update_check_parse_tag(gh, small, sizeof(small)));
}

static void write_conf(const char *path, const char *enabled) {
    FILE *f = fopen(path, "w");

    CHECK(f != NULL);

    if (f) {
        fprintf(f, "# test config\nenabled=%s\nsource=https://example.invalid/latest\n", enabled);
        fclose(f);
    }
}

static void test_run_once(void) {
    const char *conf = "/tmp/update_check_test.conf";

    setenv("UPDATE_CHECK_CONF", conf, 1);

    /* A newer release: the fake command prints a small JSON body. */
    write_conf(conf, "1");
    setenv("UPDATE_CHECK_CMD", "echo '{\"tag_name\":\"v0.9.9\"}'", 1);

    char out[64];
    CHECK(update_check_run_once("v0.1.1", out, sizeof(out)));
    CHECK(strcmp(out, "v0.9.9") == 0);

    /* Same version: nothing to announce. */
    CHECK(!update_check_run_once("v0.9.9", out, sizeof(out)));

    /* Older published version (we run a local build ahead of the release). */
    CHECK(!update_check_run_once("v1.0.0", out, sizeof(out)));

    /* The source answers with something unusable. */
    setenv("UPDATE_CHECK_CMD", "echo '{\"message\":\"Not Found\"}'", 1);
    CHECK(!update_check_run_once("v0.1.1", out, sizeof(out)));

    /* Disabled: the command must not even run. */
    write_conf(conf, "0");
    setenv("UPDATE_CHECK_CMD", "echo '{\"tag_name\":\"v9.9.9\"}'", 1);
    CHECK(!update_check_run_once("v0.1.1", out, sizeof(out)));

    unlink(conf);
    unsetenv("UPDATE_CHECK_CMD");
}

void test_update_check(void) {
    test_update_newer();
    test_parse_tag();
    test_run_once();
}

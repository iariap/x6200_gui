/*
 * Unit tests: DX cluster spot parsing and store (cluster/spots.c).
 *
 * Ported from the X6100's tests/test_cluster.cpp to this project's framework.
 * The parser is what the cluster app, the band note under the frequency and the
 * web configurator all rely on, and it is pure C, so it runs on the host.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cluster/spots.h"
#include "test_framework.h"

/* "DX de <spotter>: <freq> <dx> <comment> <HHMMZ> <locator>" */
static void test_dx_de_line(void) {
    cluster_spot_t s;

    CHECK(cluster_parse_line("DX de DL1ABC:     14023.5  VP8LP        CW 599 up 2        1238Z JO62\a\a", &s));
    CHECK(s.freq_hz == 14023500);
    CHECK(strcmp(s.dx, "VP8LP") == 0);
    CHECK(strcmp(s.spotter, "DL1ABC") == 0);
    CHECK(strcmp(s.comment, "CW 599 up 2") == 0);
    CHECK(s.utc_min == 12 * 60 + 38);

    /* A node with a -# suffix and no comment */
    CHECK(cluster_parse_line("DX de EA3CCC-#:  14205.0  PY2XB  1229Z", &s));
    CHECK(strcmp(s.spotter, "EA3CCC-#") == 0);
    CHECK(s.comment[0] == '\0');
}

/* "sh/dx" rows carry a date; live spots do not */
static void test_sh_dx_line(void) {
    cluster_spot_t s;

    CHECK(cluster_parse_line("   14196.5  ZL2ABC      29-Sep-2026 1241Z  SSB LP 57             <IU1WXY>", &s));
    CHECK(s.freq_hz == 14196500);
    CHECK(strcmp(s.dx, "ZL2ABC") == 0);
    CHECK(strcmp(s.spotter, "IU1WXY") == 0);
    CHECK(strcmp(s.comment, "SSB LP 57") == 0);
    CHECK(s.when == 1790685660);            /* 2026-09-29 12:41Z */

    /* A live spot has no date */
    CHECK(cluster_parse_line("DX de DL1ABC:  14023.5  VP8LP  CW  1238Z", &s));
    CHECK(s.when == 0);

    CHECK(cluster_parse_line("  14074.0  JA1XYZ       1-Oct-2026 0012Z FT8 -12 dB                  <DL1ABC>", &s));
    CHECK(strcmp(s.comment, "FT8 -12 dB") == 0);
    CHECK(s.when == 1790813520);

    /* Two digit year and a spotter with the -@ suffix */
    CHECK(cluster_parse_line(" 7074.0 RA0ABC      30-Sep-26 2359Z  FT8 <JA1YYY-@>", &s));
    CHECK(strcmp(s.spotter, "JA1YYY-@") == 0);
    CHECK(s.utc_min == 23 * 60 + 59);
}

/* Everything that is not a spot must be refused */
static void test_other_lines(void) {
    cluster_spot_t s;

    CHECK(!cluster_parse_line("IU1WXY de VE7CC-1 29-Sep-2026 1241Z dxspider >", &s));
    CHECK(!cluster_parse_line("WWV de VE7CC <18>:   SFI=150, A=5, K=1", &s));
    CHECK(!cluster_parse_line("login: ", &s));
    CHECK(!cluster_parse_line("", &s));
    CHECK(!cluster_parse_line("30-Sep-2026 1800Z   150   5   1  No Storms", &s));
    CHECK(!cluster_parse_line("2026 users online", &s));
}

static cluster_spot_t spot(const char *dx, uint16_t utc_min, int64_t when, const char *comment) {
    cluster_spot_t s = {0};

    s.freq_hz  = 14074000;
    s.utc_min  = utc_min;
    s.when     = when;
    snprintf(s.dx, sizeof(s.dx), "%s", dx);
    snprintf(s.comment, sizeof(s.comment), "%s", comment);

    return s;
}

/* The store keeps the spots in time order (oldest first, like the sh/dx
 * listing the server sends), replaces an older entry of the same station on the
 * same frequency and caps its size */
static void test_store(void) {
    cluster_store_t st = {0};
    cluster_spot_t  a = spot("K1AAA", 10 * 60 + 0, 0, "CW");
    cluster_spot_t  b = spot("K2BBB", 10 * 60 + 5, 0, "SSB");

    cluster_store_add(&st, &a);
    cluster_store_add(&st, &b);

    CHECK(st.count == 2);
    CHECK(strcmp(st.spots[0].dx, "K1AAA") == 0);    /* oldest first */
    CHECK(strcmp(st.spots[1].dx, "K2BBB") == 0);

    /* Adding the same station again replaces its older entry, in place */
    cluster_spot_t b2 = spot("K2BBB", 10 * 60 + 9, 0, "FT8");

    cluster_store_add(&st, &b2);
    CHECK(st.count == 2);
    CHECK(strcmp(st.spots[0].dx, "K1AAA") == 0);
    CHECK(strcmp(st.spots[1].comment, "FT8") == 0);

    /* An older spot of the same station does not replace the newer one */
    cluster_spot_t b3 = spot("K2BBB", 10 * 60 + 1, 0, "OLD");

    cluster_store_add(&st, &b3);
    CHECK(st.count == 2);
    CHECK(strcmp(st.spots[1].comment, "FT8") == 0);

    /* The store does not grow past its limit */
    cluster_store_t big = {0};

    for (int i = 0; i < CLUSTER_MAX_SPOTS + 20; i++) {
        char           dx[16];
        cluster_spot_t x;

        snprintf(dx, sizeof(dx), "K1%c", 'A' + (i % 26));
        x = spot(dx, (uint16_t) (i % 1440), 0, "CW");
        cluster_store_add(&big, &x);
    }

    CHECK(big.count <= CLUSTER_MAX_SPOTS);
}

/* Age and expiry work on minutes after 00:00Z */
static void test_age_and_expire(void) {
    cluster_store_t st = {0};
    cluster_spot_t  old = spot("K9OLD", 10 * 60 + 0, 0, "CW");
    cluster_spot_t  new_ = spot("K9NEW", 12 * 60 + 0, 0, "CW");

    cluster_store_add(&st, &old);
    cluster_store_add(&st, &new_);

    CHECK(cluster_age_min(10 * 60, 10 * 60 + 30) == 30);
    /* Across midnight the newer spot is the one with the smaller minute count */
    CHECK(cluster_age_min(23 * 60 + 50, 10) == 20);

    cluster_store_expire(&st, 12 * 60 + 30, 60);    /* 60 minutes */

    CHECK(st.count == 1);
    CHECK(strcmp(st.spots[0].dx, "K9NEW") == 0);
}

/* Band lookup and mode guess */
static void test_band_and_mode(void) {
    CHECK(cluster_band_of(14074000) == cluster_band_of(14000000));
    CHECK(strcmp(cluster_band_name(cluster_band_of(14074000)), "20m") == 0);
    CHECK(strcmp(cluster_band_name(cluster_band_of(7038600)), "40m") == 0);
    CHECK(cluster_band_of(12000000) == -1);             /* between bands */
    CHECK(strcmp(cluster_band_name(-1), "") == 0);   /* no band: empty name */

    cluster_spot_t cw = spot("K1AAA", 0, 0, "CW 599");

    CHECK(cluster_guess_mode(&cw) == CLUSTER_MODE_CW);

    cluster_spot_t ssb = spot("K1AAA", 0, 0, "SSB 59");

    CHECK(cluster_guess_mode(&ssb) == CLUSTER_MODE_USB);

    cluster_spot_t ft8 = spot("K1AAA", 0, 0, "FT8 -12 dB");

    CHECK(cluster_guess_mode(&ft8) == CLUSTER_MODE_DIGI);
}

void test_cluster(void) {
    test_dx_de_line();
    test_sh_dx_line();
    test_other_lines();
    test_store();
    test_age_and_expire();
    test_band_and_mode();
}

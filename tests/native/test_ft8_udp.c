/*
 * Unit tests: WSJT-X compatible UDP broadcaster (ft8_udp.c).
 *
 * Covers the ADIF record builder, the ADIF band mapping and a real datagram
 * exchange over the loopback interface, validating the WSJT-X wire format
 * (magic, schema, message type, station id and payload).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "ft8_udp.h"
#include "test_framework.h"

#define WSJTX_MAGIC 0xadbccbdaU

static uint32_t rd32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) |
           ((uint32_t) p[2] << 8) | (uint32_t) p[3];
}

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

static void test_band_names(void) {
    CHECK(strcmp(ft8_udp_band_name(1840000ULL), "160m") == 0);
    CHECK(strcmp(ft8_udp_band_name(3573000ULL), "80m") == 0);
    CHECK(strcmp(ft8_udp_band_name(5357000ULL), "60m") == 0);
    CHECK(strcmp(ft8_udp_band_name(7074000ULL), "40m") == 0);
    CHECK(strcmp(ft8_udp_band_name(10136000ULL), "30m") == 0);
    CHECK(strcmp(ft8_udp_band_name(14074000ULL), "20m") == 0);
    CHECK(strcmp(ft8_udp_band_name(18100000ULL), "17m") == 0);
    CHECK(strcmp(ft8_udp_band_name(21074000ULL), "15m") == 0);
    CHECK(strcmp(ft8_udp_band_name(24915000ULL), "12m") == 0);
    CHECK(strcmp(ft8_udp_band_name(28074000ULL), "10m") == 0);
    CHECK(strcmp(ft8_udp_band_name(50313000ULL), "6m") == 0);

    /* Out of band: empty string, never NULL. */
    CHECK(ft8_udp_band_name(1000000ULL) != NULL);
    CHECK(strcmp(ft8_udp_band_name(1000000ULL), "") == 0);
}

static void test_adif_record(void) {
    char   out[1024];
    size_t n = ft8_udp_build_adif(out, sizeof(out), 1700000000 /* 2023-11-14 22:13:20Z */,
                                  "K1ABC", "FN42", 14074000, "FT8",
                                  -10, -12, 5, "LU2DSX", "GF05");

    CHECK(n > 0);
    CHECK(strstr(out, "X6200 ADIF export") != NULL);
    CHECK(strstr(out, "<adif_ver:5>3.1.0") != NULL);
    CHECK(strstr(out, "<call:5>K1ABC") != NULL);
    CHECK(strstr(out, "<gridsquare:4>FN42") != NULL);
    CHECK(strstr(out, "<mode:3>FT8") != NULL);
    CHECK(strstr(out, "<band:3>20m") != NULL);
    CHECK(strstr(out, "<freq:9>14.074000") != NULL);
    CHECK(strstr(out, "<rst_sent:-10>") == NULL);       /* ADIF: no length on numbers */
    CHECK(strstr(out, "<rst_sent:3>-10") != NULL);
    CHECK(strstr(out, "<station_callsign:6>LU2DSX") != NULL);
    CHECK(strstr(out, "<tx_pwr:1>5") != NULL);
    CHECK(strstr(out, "<eor>") != NULL);
    CHECK(strstr(out, "20231114") != NULL);

    /* Truncated buffer must not overflow and must return a sane length. */
    char small[64];
    size_t m = ft8_udp_build_adif(small, sizeof(small), 1700000000, "K1ABC", "FN42",
                                  14074000, "FT8", -10, -12, 5, "LU2DSX", "GF05");
    CHECK(m < sizeof(small));
}

static int open_rx_socket(uint16_t *port_out) {
    struct sockaddr_in me = {0};
    socklen_t          sl = sizeof(me);
    int                rx = socket(AF_INET, SOCK_DGRAM, 0);

    if (rx < 0) {
        return -1;
    }

    me.sin_family      = AF_INET;
    me.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    me.sin_port        = 0;

    if (bind(rx, (struct sockaddr *) &me, sizeof(me)) != 0 ||
        getsockname(rx, (struct sockaddr *) &me, &sl) != 0) {
        close(rx);
        return -1;
    }

    /* Never hang the test suite if a datagram does not arrive. */
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(rx, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    *port_out = ntohs(me.sin_port);

    return rx;
}

static void test_datagrams(void) {
    const char *conf = "/tmp/ft8_udp_test.conf";
    uint16_t    port = 0;
    int         rx   = open_rx_socket(&port);
    uint8_t     buf[2048];
    ssize_t     n;

    CHECK(rx >= 0);

    if (rx < 0) {
        return;
    }

    FILE *f = fopen(conf, "w");

    CHECK(f != NULL);

    if (f) {
        fprintf(f, "# test\nenabled=1\nhost=127.0.0.1\nport=%u\nid=X6200\n", port);
        fclose(f);
    }

    setenv("FT8_UDP_CONF", conf, 1);

    ft8_udp_init();

    CHECK(ft8_udp_is_enabled());
    CHECK(ft8_udp_port() == port);
    CHECK(strcmp(ft8_udp_host(), "127.0.0.1") == 0);

    /* --- Heartbeat (type 0) --- */
    ft8_udp_send_heartbeat();

    n = recv(rx, buf, sizeof(buf), 0);

    CHECK(n > 20);

    if (n > 20) {
        CHECK(rd32(buf) == WSJTX_MAGIC);
        CHECK(rd32(buf + 4) == 3);              /* schema */
        CHECK(rd32(buf + 8) == 0);              /* heartbeat */
        CHECK(rd32(buf + 12) == 5);             /* id length */
        CHECK(memcmp(buf + 16, "X6200", 5) == 0);
        CHECK(contains(buf, (size_t) n, "2.6.1"));
    }

    /* --- Decode (type 2) --- */
    ft8_udp_send_decode(true, "FT8", "CQ LU2DSX GF05", -12, 0.1f, 1500);

    n = recv(rx, buf, sizeof(buf), 0);

    CHECK(n > 30);

    if (n > 30) {
        CHECK(rd32(buf) == WSJTX_MAGIC);
        CHECK(rd32(buf + 8) == 2);              /* decode */
        CHECK(contains(buf, (size_t) n, "FT8"));
        CHECK(contains(buf, (size_t) n, "CQ LU2DSX GF05"));
    }

    /* --- Status (type 1) --- */
    ft8_udp_send_status(14074000, "FT8", "LU2DSX", "GF05", "K1ABC", 1500, 1500, false, true);

    n = recv(rx, buf, sizeof(buf), 0);

    CHECK(n > 30);

    if (n > 30) {
        CHECK(rd32(buf + 8) == 1);              /* status */
        CHECK(contains(buf, (size_t) n, "LU2DSX"));
        CHECK(contains(buf, (size_t) n, "K1ABC"));
        CHECK(contains(buf, (size_t) n, "GF05"));
    }

    /* --- QSO logged: structured (5) plus ADIF (12) --- */
    ft8_udp_log_qso(1700000000, "K1ABC", "FN42", 14074000, "FT8", -10, -12, 5,
                    "LU2DSX", "GF05");

    bool saw_qso_logged = false;
    bool saw_logged_adif = false;

    for (int i = 0; i < 2; i++) {
        n = recv(rx, buf, sizeof(buf), 0);

        if (n <= 12) {
            continue;
        }

        uint32_t type = rd32(buf + 8);

        if (type == 5) {
            saw_qso_logged = true;
            CHECK(contains(buf, (size_t) n, "K1ABC"));
        } else if (type == 12) {
            saw_logged_adif = true;
            CHECK(contains(buf, (size_t) n, "<call:5>K1ABC"));
            CHECK(contains(buf, (size_t) n, "<band:3>20m"));
        }
    }

    CHECK(saw_qso_logged);
    CHECK(saw_logged_adif);

    /* --- Disabled: nothing must be sent --- */
    ft8_udp_set_enabled(false);
    ft8_udp_send_heartbeat();

    n = recv(rx, buf, sizeof(buf), 0);
    CHECK(n < 0);                               /* timeout: no datagram */

    ft8_udp_deinit();
    close(rx);
    unlink(conf);
}

void test_ft8_udp(void) {
    test_band_names();
    test_adif_record();
    test_datagrams();
}

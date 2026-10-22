/*
 * Unit tests: wfview (Icom LAN) server protocol and configuration.
 *
 * The real src/wfview_server.c is compiled for the host with stubs for the two
 * things it talks to outside its own UDP sockets: the CAT engine (which the
 * tests replace with a canned CI-V reply and a captured "network sink") and
 * PulseAudio (only used by the audio thread, which the tests never drive).
 *
 * The tests then speak the protocol over loopback UDP: handshake, login with
 * the real password encoding, rejection of bad credentials, the CI-V bridge and
 * the unsolicited transceive sink.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pulse/simple.h"
#include "test_framework.h"
#include "wfview_server.h"

/* ------------------------------------------------------------------ stubs */

/* A CI-V answer the fake CAT engine returns for any request */
static const uint8_t cat_reply[] = { 0xFE, 0xFE, 0xE0, 0xA4, 0x00, 0x00, 0xFD };
static void (*test_net_sink)(const uint8_t *data, size_t len);

size_t cat_process_frame(const uint8_t *req, size_t len, uint8_t *resp, size_t resp_max) {
    (void) req;

    if (len == 0 || resp_max < sizeof(cat_reply)) {
        return 0;
    }

    memcpy(resp, cat_reply, sizeof(cat_reply));

    return sizeof(cat_reply);
}

void cat_set_net_sink(void (*sink)(const uint8_t *data, size_t len)) {
    test_net_sink = sink;
}

pa_simple *pa_simple_new(const char *server, const char *name, pa_stream_direction_t dir,
                         const char *dev, const char *stream_name, const pa_sample_spec *ss,
                         const pa_channel_map *map, const pa_buffer_attr *attr, int *error) {
    (void) server; (void) name; (void) dir; (void) dev; (void) stream_name;
    (void) ss; (void) map; (void) attr;

    if (error) {
        *error = -1;
    }

    return NULL;
}

int pa_simple_read(pa_simple *s, void *data, size_t bytes, int *error) {
    (void) s; (void) data; (void) bytes;

    if (error) {
        *error = -1;
    }

    return -1;
}

void pa_simple_free(pa_simple *s) {
    (void) s;
}

/* ------------------------------------------------------------------ utils */

#define CTRL_PORT   50001
#define CIV_PORT    50002
#define CONF_PATH   "/tmp/wfview_test.conf"

static int sock_fd = -1;

static void put16le(uint8_t *p, uint16_t v) {
    p[0] = v;
    p[1] = v >> 8;
}

static void put32le(uint8_t *p, uint32_t v) {
    put16le(p, v);
    put16le(p + 2, v >> 16);
}

static uint16_t get16le(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static uint32_t get32le(const uint8_t *p) {
    return get16le(p) | ((uint32_t) get16le(p + 2) << 16);
}

/* Fill the 16-byte header every packet starts with */
static void hdr(uint8_t *pkt, uint32_t len, uint16_t type, uint16_t seq, uint32_t my_id) {
    put32le(pkt, len);
    put16le(pkt + 4, type);
    put16le(pkt + 6, seq);
    put32le(pkt + 8, my_id);
    put32le(pkt + 0xc, 0x11223344);
}

static void send_to_port(uint16_t port, const uint8_t *pkt, size_t len) {
    struct sockaddr_in to = {
        .sin_family = AF_INET,
        .sin_port   = htons(port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };

    sendto(sock_fd, pkt, len, 0, (struct sockaddr *) &to, sizeof(to));
}

/* Returns the packet length, or -1 on timeout */
static int recv_pkt(uint8_t *buf, size_t max) {
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ssize_t n = recvfrom(sock_fd, buf, max, 0, NULL, NULL);

    return (int) n;
}

static void write_conf(const char *user, const char *pass, bool enabled) {
    FILE *f = fopen(CONF_PATH, "w");

    if (!f) {
        return;
    }

    fprintf(f, "enabled=%d\nuser=%s\npass=%s\n", enabled ? 1 : 0, user, pass);
    fclose(f);
}

/* ------------------------------------------------------------------ tests */

static void test_config_and_defaults(void) {
    write_conf("tester", "secret", false);
    setenv("WFVIEW_CONF", CONF_PATH, 1);

    wfview_server_init();

    CHECK(!wfview_server_get_enabled());
    CHECK(strcmp(wfview_server_user(), "tester") == 0);
    CHECK(strcmp(wfview_server_pass(), "secret") == 0);
    CHECK(wfview_server_client() == NULL);

    /* Enabling rewrites the file, so the web configurator and the GUI agree */
    wfview_server_set_enabled(true);
    CHECK(wfview_server_get_enabled());

    FILE *f = fopen(CONF_PATH, "r");
    char  line[64];
    bool  file_says_on = false;

    CHECK(f != NULL);

    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "enabled=1", 9) == 0) {
                file_says_on = true;
            }
        }

        fclose(f);
    }

    CHECK(file_says_on);
}

static void test_protocol_handshake(void) {
    uint8_t buf[2048];
    uint8_t pkt[0x90];

    /* "are you there" */
    memset(pkt, 0, sizeof(pkt));
    hdr(pkt, 0x10, 0x03, 0x0001, 0x00000001);
    send_to_port(CTRL_PORT, pkt, 0x10);

    int n = recv_pkt(buf, sizeof(buf));

    CHECK(n == 0x10);

    if (n == 0x10) {
        CHECK(get16le(buf + 4) == 0x04);
    }

    /* "are you ready" */
    memset(pkt, 0, sizeof(pkt));
    hdr(pkt, 0x10, 0x06, 0x0002, 0x00000001);
    send_to_port(CTRL_PORT, pkt, 0x10);

    n = recv_pkt(buf, sizeof(buf));
    CHECK(n == 0x10);

    if (n == 0x10) {
        CHECK(get16le(buf + 4) == 0x06);
    }

    /* Ping is answered with our sequence number echoed back */
    memset(pkt, 0, sizeof(pkt));
    hdr(pkt, 0x15, 0x07, 0x0003, 0x00000001);
    pkt[0x10] = 0x00;
    put32le(pkt + 0x11, 0xAABBCCDD);
    send_to_port(CTRL_PORT, pkt, 0x15);

    n = recv_pkt(buf, sizeof(buf));
    CHECK(n == 0x15);

    if (n == 0x15) {
        CHECK(get16le(buf + 4) == 0x07);
        CHECK(buf[0x10] == 0x01);
        CHECK(get32le(buf + 0x11) == 0xAABBCCDD);
    }
}

static void send_login(const char *user, const char *pass, const char *name) {
    uint8_t pkt[0x80];

    memset(pkt, 0, sizeof(pkt));
    hdr(pkt, sizeof(pkt), 0x00, 0x0010, 0x00000001);
    put16le(pkt + 0x1a, 0x1234);                  /* token request */
    wfview_server_passcode(user, pkt + 0x40);
    wfview_server_passcode(pass, pkt + 0x50);
    snprintf((char *) pkt + 0x60, 16, "%s", name);
    send_to_port(CTRL_PORT, pkt, sizeof(pkt));
}

static void test_login(void) {
    uint8_t buf[2048];

    /* Wrong password: rejected, and no client is reported */
    send_login("tester", "WRONG", "BADCLIENT");

    int n = recv_pkt(buf, sizeof(buf));

    CHECK(n == 0x60);

    if (n == 0x60) {
        CHECK(get16le(buf + 4) == 0x00);
        CHECK(get32le(buf + 0x30) != 0);           /* error code */
    }

    CHECK(wfview_server_client() == NULL);

    /* Right credentials: accepted and the client name is published */
    send_login("tester", "secret", "TESTCLIENT");

    n = recv_pkt(buf, sizeof(buf));
    CHECK(n == 0x60);

    if (n == 0x60) {
        CHECK(get16le(buf + 4) == 0x00);
        CHECK(get32le(buf + 0x30) == 0);
        CHECK(memcmp(buf + 0x40, "WFVIEW", 6) == 0);
    }

    const char *client = wfview_server_client();

    CHECK(client != NULL);

    if (client) {
        CHECK(strcmp(client, "TESTCLIENT") == 0);
    }
}

static void test_civ_bridge(void) {
    uint8_t buf[2048];
    uint8_t pkt[0x90];
    uint8_t frame[] = { 0xFE, 0xFE, 0xA4, 0xE0, 0x03, 0xFD };

    /* The CI-V port needs its own handshake before it accepts data */
    memset(pkt, 0, sizeof(pkt));
    hdr(pkt, 0x10, 0x03, 0x0020, 0x00000001);
    send_to_port(CIV_PORT, pkt, 0x10);
    recv_pkt(buf, sizeof(buf));

    memset(pkt, 0, sizeof(pkt));
    hdr(pkt, 0x10, 0x06, 0x0021, 0x00000001);
    send_to_port(CIV_PORT, pkt, 0x10);
    recv_pkt(buf, sizeof(buf));

    /* A CI-V frame from the client goes to the CAT engine and its answer back */
    memset(pkt, 0, sizeof(pkt));
    uint32_t len = 0x15 + sizeof(frame);

    hdr(pkt, len, 0x00, 0x0022, 0x00000001);
    pkt[0x10] = 0xC1;
    put16le(pkt + 0x11, sizeof(frame));
    memcpy(pkt + 0x15, frame, sizeof(frame));
    send_to_port(CIV_PORT, pkt, len);

    int n = recv_pkt(buf, sizeof(buf));

    CHECK(n > 0x15);

    if (n > 0x15) {
        CHECK(buf[0x10] == 0xC1);
        CHECK(get16le(buf + 0x11) == sizeof(cat_reply));
        CHECK(memcmp(buf + 0x15, cat_reply, sizeof(cat_reply)) == 0);
    }

    /* Unsolicited transceive frames reach the network through the sink that the
     * server installed in the CAT engine */
    CHECK(test_net_sink != NULL);

    if (test_net_sink) {
        uint8_t tx[] = { 0xFE, 0xFE, 0xE0, 0xA4, 0x00, 0x14, 0x07, 0x04, 0xFD };

        test_net_sink(tx, sizeof(tx));

        n = recv_pkt(buf, sizeof(buf));
        CHECK(n == 0x15 + (int) sizeof(tx));

        if (n == 0x15 + (int) sizeof(tx)) {
            CHECK(memcmp(buf + 0x15, tx, sizeof(tx)) == 0);
        }
    }
}

static void test_ports_are_bound(void) {
    /* The server must be listening: a second bind on the control port fails */
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    CHECK(fd >= 0);

    if (fd < 0) {
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(CTRL_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    int r = bind(fd, (struct sockaddr *) &addr, sizeof(addr));

    CHECK(r != 0);
    CHECK(errno == EADDRINUSE);
    close(fd);
}

void test_wfview(void) {
    /* Ports may be taken by something else on a busy host: skip the network part */
    int probe = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(CTRL_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    bool busy = probe < 0 || bind(probe, (struct sockaddr *) &addr, sizeof(addr)) != 0;

    if (probe >= 0) {
        close(probe);
    }

    test_config_and_defaults();

    if (busy) {
        printf("  (puertos %d-%d ocupados: se omite la parte de red)\n", CTRL_PORT, CIV_PORT + 1);
        unlink(CONF_PATH);
        return;
    }

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    CHECK(sock_fd >= 0);

    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port   = 0,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };

    CHECK(bind(sock_fd, (struct sockaddr *) &local, sizeof(local)) == 0);

    /* Let the server threads reach their poll loop */
    usleep(200000);

    test_ports_are_bound();
    test_protocol_handshake();
    test_login();
    test_civ_bridge();

    close(sock_fd);
    sock_fd = -1;
    unlink(CONF_PATH);
}

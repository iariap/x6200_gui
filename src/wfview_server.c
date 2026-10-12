/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Ported from the X6100 firmware by IU1WXY (LGPL-2.1-or-later).
 *  Battery: the net/audio threads idle with long sleeps while the server is
 *  disabled (the X6100 polls at 10 Hz even then).
 *
 *  wfview server: Icom LAN UDP protocol (control 50001, CI-V 50002, audio 50003).
 *  One client, RX audio LPCM 16-bit mono.
 */

#include "wfview_server.h"
#include "cat.h"

#include "lvgl/lvgl.h"
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <pulse/simple.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define CONF_FILE   "/mnt/wfview.conf"
#define RADIO_NAME  "IC-705"
#define CIV_ADDR    0xA4
#define HIST_LEN    64
#define PKT_MAX     1500
#define STALE_MS    15000
#define CODEC_LPCM16 0x04

enum { P_CTRL, P_CIV, P_AUDIO, P_NUM };

typedef struct {
    int                fd;
    uint16_t           port;
    uint32_t           my_id;
    uint32_t           rem_id;
    struct sockaddr_in peer;
    bool               connected;
    uint16_t           seq;
    uint16_t           inner_seq;
    uint8_t            hist[HIST_LEN][PKT_MAX];
    uint16_t           hist_len[HIST_LEN];
    uint16_t           hist_seq[HIST_LEN];
    pthread_mutex_t    lock;
} port_t;

static port_t   ports[P_NUM];
static char     user[16], pass[16];
static uint8_t  tokrequest[2], token[4], guid[16];
static char     client_name[16];
static bool     enabled, logged;
static uint64_t last_rx_ms;
static volatile bool     streaming;
static volatile uint32_t rx_rate;

static const uint8_t passtab[127] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x47, 0x5d, 0x4c, 0x42, 0x66, 0x20, 0x23, 0x46, 0x4e, 0x57, 0x45, 0x3d, 0x67, 0x76, 0x60, 0x41,
    0x62, 0x39, 0x59, 0x2d, 0x68, 0x7e, 0x7c, 0x65, 0x7d, 0x49, 0x29, 0x72, 0x73, 0x78, 0x21, 0x6e,
    0x5a, 0x5e, 0x4a, 0x3e, 0x71, 0x2c, 0x2a, 0x54, 0x3c, 0x3a, 0x63, 0x4f, 0x43, 0x75, 0x27, 0x79,
    0x5b, 0x35, 0x70, 0x48, 0x6b, 0x56, 0x6f, 0x34, 0x32, 0x6c, 0x30, 0x61, 0x6d, 0x7b, 0x2f, 0x4b,
    0x64, 0x38, 0x2b, 0x2e, 0x50, 0x40, 0x3f, 0x55, 0x33, 0x37, 0x25, 0x77, 0x24, 0x26, 0x74, 0x6a,
    0x28, 0x53, 0x4d, 0x69, 0x22, 0x5c, 0x44, 0x31, 0x36, 0x58, 0x3b, 0x7a, 0x51, 0x5f, 0x52,
};

static void passcode(const char *in, uint8_t out[16]) {
    memset(out, 0, 16);
    for (int i = 0; i < 16 && in[i]; i++) {
        int p = (uint8_t)in[i] + i;
        if (p > 126)
            p = 32 + p % 127;
        out[i] = passtab[p];
    }
}

static uint64_t now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000;
}

static void put16le(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put32le(uint8_t *p, uint32_t v) { put16le(p, v); put16le(p + 2, v >> 16); }
static void put16be(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
static void put32be(uint8_t *p, uint32_t v) { put16be(p, v >> 16); put16be(p + 2, v); }
static uint16_t get16le(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t get32le(const uint8_t *p) { return get16le(p) | (uint32_t)get16le(p + 2) << 16; }
static uint32_t get32be(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

/* Fill the common header; tracked packets get the next seq and are kept for retransmit */
static void send_pkt(port_t *pt, uint8_t *pkt, uint16_t len, uint16_t type, bool tracked, uint16_t seq) {
    pthread_mutex_lock(&pt->lock);
    if (tracked) {
        if (++pt->seq == 0)
            pt->seq = 1;
        seq = pt->seq;
    }
    put32le(pkt, len);
    put16le(pkt + 4, type);
    put16le(pkt + 6, seq);
    put32le(pkt + 8, pt->my_id);
    put32le(pkt + 0xc, pt->rem_id);
    if (tracked) {
        int i = seq % HIST_LEN;
        memcpy(pt->hist[i], pkt, len);
        pt->hist_len[i] = len;
        pt->hist_seq[i] = seq;
    }
    sendto(pt->fd, pkt, len, 0, (struct sockaddr *)&pt->peer, sizeof(pt->peer));
    pthread_mutex_unlock(&pt->lock);
}

static void send_ctrl(port_t *pt, uint16_t type, bool tracked, uint16_t seq) {
    uint8_t pkt[0x10] = {0};
    send_pkt(pt, pkt, sizeof(pkt), type, tracked, seq);
}

static void send_civ(const uint8_t *data, size_t len) {
    port_t *pt = &ports[P_CIV];
    uint8_t pkt[PKT_MAX] = {0};

    if (!pt->connected || len > PKT_MAX - 0x15)
        return;
    pkt[0x10] = 0xC1;
    put16le(pkt + 0x11, len);
    put16be(pkt + 0x13, pt->inner_seq++);
    memcpy(pkt + 0x15, data, len);
    send_pkt(pt, pkt, 0x15 + len, 0, true, 0);
}

/* Control packets with the 0x10..0x2f token block */
static void token_block(uint8_t *pkt, uint16_t len, uint8_t reply, uint8_t type, const uint8_t *req) {
    put32be(pkt + 0x10, len - 0x10);
    pkt[0x14] = reply;
    pkt[0x15] = type;
    if (req)
        memcpy(pkt + 0x16, req + 0x16, 2);
    memcpy(pkt + 0x1a, tokrequest, 2);
    memcpy(pkt + 0x1c, token, 4);
}

static void send_conninfo(bool busy) {
    port_t *pt = &ports[P_CTRL];
    uint8_t pkt[0x90] = {0};

    token_block(pkt, sizeof(pkt), 0x03, 0x00, NULL);
    memcpy(pkt + 0x20, guid, 16);
    strcpy((char *)pkt + 0x40, RADIO_NAME);
    put32le(pkt + 0x60, busy);
    if (busy) {
        memcpy(pkt + 0x64, client_name, 16);
        memcpy(pkt + 0x84, &pt->peer.sin_addr.s_addr, 4);
    }
    send_pkt(pt, pkt, sizeof(pkt), 0, true, 0);
}

static void disconnect_all() {
    streaming = false;
    logged    = false;
    for (int i = 0; i < P_NUM; i++)
        ports[i].connected = false;
    LV_LOG_USER("wfview: client disconnected");
}

static void on_login(const uint8_t *req) {
    port_t *pt = &ports[P_CTRL];
    uint8_t pkt[0x60] = {0};
    uint8_t enc_user[16], enc_pass[16];

    passcode(user, enc_user);
    passcode(pass, enc_pass);
    bool ok = !memcmp(req + 0x40, enc_user, 16) && !memcmp(req + 0x50, enc_pass, 16);

    memcpy(tokrequest, req + 0x1a, 2);
    for (int i = 0; i < 4; i++)
        token[i] = rand();
    memcpy(client_name, req + 0x60, 16);
    client_name[15] = 0;
    logged = ok;

    token_block(pkt, sizeof(pkt), 0x01, 0x00, req);
    put32le(pkt + 0x30, ok ? 0 : 0xFEFFFFFF);
    strcpy((char *)pkt + 0x40, "WFVIEW");
    send_pkt(pt, pkt, sizeof(pkt), 0, true, 0);
    LV_LOG_USER("wfview: login from %s %s", client_name, ok ? "ok" : "rejected");
}

static void send_caps(const uint8_t *req) {
    uint8_t pkt[0x42 + 0x66] = {0};
    uint8_t *cap = pkt + 0x42;

    token_block(pkt, sizeof(pkt), 0x02, 0x02, req);
    put16be(pkt + 0x40, 1);
    put16le(cap + 0x07, 0x8010);
    memcpy(cap + 0x0a, "\x88\x49\x2d\x00\x61\x00", 6);
    strcpy((char *)cap + 0x10, RADIO_NAME);
    strcpy((char *)cap + 0x30, "ICOM_VAUDIO");
    put16le(cap + 0x50, 0x0707);
    cap[0x52] = CIV_ADDR;
    put16le(cap + 0x53, 0x8b01);
    put16le(cap + 0x55, 0);     /* RX only */
    cap[0x57] = cap[0x58] = cap[0x59] = 1;
    put32be(cap + 0x5a, 115200);
    put16le(cap + 0x5e, 0x5001);
    put16le(cap + 0x61, 0x0190);
    memcpy(guid, cap, 16);
    send_pkt(&ports[P_CTRL], pkt, sizeof(pkt), 0, true, 0);
}

static void on_stream_req(const uint8_t *req) {
    uint8_t  pkt[0x50] = {0};
    uint8_t  codec     = req[0x72];
    uint32_t rate      = get32be(req + 0x74);
    bool     ok        = codec == CODEC_LPCM16 && rate >= 8000 && rate <= 48000;

    token_block(pkt, sizeof(pkt), 0x02, 0x03, req);
    memcpy(pkt + 0x20, req + 0x20, 16);
    put32le(pkt + 0x30, ok ? 0 : 0xFFFFFFFF);
    put16be(pkt + 0x42, ports[P_CIV].port);
    put16be(pkt + 0x46, ports[P_AUDIO].port);
    send_pkt(&ports[P_CTRL], pkt, sizeof(pkt), 0, true, 0);
    LV_LOG_USER("wfview: stream codec %02X rate %u %s", codec, rate, ok ? "ok" : "rejected (use LPCM 1ch 16bit)");
    if (ok) {
        rx_rate   = rate;
        streaming = true;
        send_conninfo(true);
    }
}

static void on_token(const uint8_t *req, uint16_t len) {
    uint8_t pkt[0x40] = {0};
    uint8_t type      = req[0x15];

    if (len < 0x40)
        return;
    if (type == 0x02) {
        send_caps(req);
        send_conninfo(false);
        return;
    }
    token_block(pkt, sizeof(pkt), 0x02, type, req);
    memcpy(pkt + 0x20, req + 0x20, 16);
    send_pkt(&ports[P_CTRL], pkt, sizeof(pkt), 0, true, 0);
    if (type == 0x04) {
        streaming = false;
        send_conninfo(false);
    }
}

static void on_packet(int n, const uint8_t *pkt, uint16_t len, struct sockaddr_in *from) {
    port_t  *pt   = &ports[n];
    uint16_t type = get16le(pkt + 4);
    uint16_t seq  = get16le(pkt + 6);

    if (!enabled) {
        if (ports[P_CTRL].connected)
            disconnect_all();
        return;
    }
    if (len < 0x10 || get32le(pkt) != len)
        return;
    /* CI-V and audio only from the logged-in client's address */
    if (n != P_CTRL && (!ports[P_CTRL].connected || from->sin_addr.s_addr != ports[P_CTRL].peer.sin_addr.s_addr))
        return;

    last_rx_ms = now_ms();

    switch (type) {
        case 0x03:  /* are you there */
            pt->peer    = *from;
            pt->rem_id  = get32le(pkt + 8);
            pt->seq     = 0;
            send_ctrl(pt, 0x04, false, seq);
            return;
        case 0x06:  /* are you ready */
            pt->rem_id    = get32le(pkt + 8);
            pt->connected = true;
            send_ctrl(pt, 0x06, false, seq);
            return;
        case 0x05:
            send_ctrl(pt, 0x00, false, seq);
            disconnect_all();
            return;
        case 0x07:  /* ping */
            if (len >= 0x15 && pkt[0x10] == 0) {
                uint8_t r[0x15] = {0};
                r[0x10] = 1;
                memcpy(r + 0x11, pkt + 0x11, 4);
                send_pkt(pt, r, sizeof(r), 0x07, false, seq);
            }
            return;
        case 0x01:  /* retransmit request */
            for (uint16_t i = (len == 0x10 ? 0x06 : 0x10); i + 2 <= len; i += 2) {
                uint16_t s = get16le(pkt + i);
                int      h = s % HIST_LEN;
                pthread_mutex_lock(&pt->lock);
                bool found = pt->hist_seq[h] == s && pt->hist_len[h];
                if (found)
                    sendto(pt->fd, pt->hist[h], pt->hist_len[h], 0, (struct sockaddr *)&pt->peer, sizeof(pt->peer));
                pthread_mutex_unlock(&pt->lock);
                if (!found)
                    send_ctrl(pt, 0x00, false, s);
                if (len == 0x10)
                    break;
            }
            return;
        case 0x00:
            break;
        default:
            return;
    }

    if (n == P_CTRL) {
        if (len == 0x80)
            on_login(pkt);
        else if (len == 0x40)
            on_token(pkt, len);
        else if (len == 0x90 && pkt[0x15] == 0x03)
            on_stream_req(pkt);
    } else if (n == P_CIV && len > 0x18 && get16le(pkt + 0x11) + 0x15 == len) {
        /* A packet may carry several CI-V frames (FE FE .. FD) */
        const uint8_t *p   = pkt + 0x15;
        const uint8_t *end = pkt + len;
        while (p < end) {
            const uint8_t *fd = memchr(p, 0xFD, end - p);
            if (!fd)
                break;
            uint8_t resp[PKT_MAX];
            size_t  l = cat_process_frame(p, fd - p + 1, resp, sizeof(resp));
            if (l)
                send_civ(resp, l);
            p = fd + 1;
        }
    }
}

static void *audio_thread(void *arg) {
    pa_simple *s = NULL;
    uint8_t    pkt[0x18 + 1364];
    uint16_t   sendseq = 0;
    uint32_t   rate    = 0;

    while (true) {
        if (!streaming || rate != rx_rate) {
            if (s) {
                pa_simple_free(s);
                s = NULL;
            }
            if (!streaming) {
                usleep(300000);         /* idle: 3 wakes/s instead of 20 */
                continue;
            }
        }
        if (!s) {
            rate = rx_rate;
            pa_sample_spec spec = {.format = PA_SAMPLE_S16LE, .rate = rate, .channels = 1};
            pa_buffer_attr attr = {.maxlength = -1, .fragsize = rate / 50 * 2};
            s = pa_simple_new(NULL, "X6200 wfview", PA_STREAM_RECORD, "alsa_input.platform-sound.stereo-fallback",
                              "wfview RX", &spec, NULL, &attr, NULL);
            if (!s) {
                LV_LOG_ERROR("wfview: pa_simple_new failed");
                sleep(1);
                continue;
            }
        }
        /* 20 ms blocks, split at 1364 bytes */
        size_t block = rate / 50 * 2;
        for (size_t off = 0; off < block;) {
            size_t n = block - off > 1364 ? 1364 : block - off;
            if (pa_simple_read(s, pkt + 0x18, n, NULL) < 0)
                break;
            memset(pkt + 0x10, 0, 8);
            put16le(pkt + 0x10, 0x0080);
            put16be(pkt + 0x12, sendseq++);
            put16be(pkt + 0x16, n);
            if (ports[P_AUDIO].connected)
                send_pkt(&ports[P_AUDIO], pkt, 0x18 + n, 0, true, 0);
            off += n;
        }
    }
    return NULL;
}

static void load_conf();

/* Config path; $WFVIEW_CONF overrides it so the tests do not touch /mnt */
static const char *conf_path(void) {
    const char *p = getenv("WFVIEW_CONF");

    return p && *p ? p : CONF_FILE;
}

/* Reload the config when the web configurator rewrites it */
static void check_conf() {
    static time_t mtime;
    struct stat   st;

    if (stat(conf_path(), &st) || st.st_mtime == mtime)
        return;
    mtime = st.st_mtime;
    load_conf();
    if (!enabled && ports[P_CTRL].connected)
        disconnect_all();
}

static void *net_thread(void *arg) {
    struct pollfd pfd[P_NUM];
    uint8_t       buf[PKT_MAX];
    uint64_t      next_idle = 0, next_conf = 0;

    for (int i = 0; i < P_NUM; i++)
        pfd[i] = (struct pollfd){.fd = ports[i].fd, .events = POLLIN};

    while (true) {
        /* Idle cheaply while the server is off: nothing to keep alive */
        poll(pfd, P_NUM, enabled ? 100 : 1000);
        for (int i = 0; i < P_NUM; i++) {
            if (!(pfd[i].revents & POLLIN))
                continue;
            struct sockaddr_in from;
            socklen_t          fl = sizeof(from);
            ssize_t            l  = recvfrom(ports[i].fd, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fl);
            if (l > 0)
                on_packet(i, buf, l, &from);
        }
        uint64_t now = now_ms();
        if (ports[P_CTRL].connected && now - last_rx_ms > STALE_MS)
            disconnect_all();
        if (now >= next_conf) {
            next_conf = now + 2000;
            check_conf();
        }
        if (enabled && now >= next_idle) {
            next_idle = now + 100;
            for (int i = P_CTRL; i <= P_CIV; i++)
                if (ports[i].connected)
                    send_ctrl(&ports[i], 0x00, true, 0);
        }
    }
    return NULL;
}

static void load_conf() {
    char  line[64];
    FILE *f = fopen(conf_path(), "r");

    strcpy(user, "x6200");
    strcpy(pass, "x6200");
    enabled = false;
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "user=", 5))
            snprintf(user, sizeof(user), "%.*s", (int) sizeof(user) - 1, line + 5);
        else if (!strncmp(line, "pass=", 5))
            snprintf(pass, sizeof(pass), "%.*s", (int) sizeof(pass) - 1, line + 5);
        else if (!strncmp(line, "enabled=", 8))
            enabled = line[8] == '1';
    }
    fclose(f);
}

static void save_conf() {
    FILE *f = fopen(conf_path(), "w");

    if (!f)
        return;
    fprintf(f, "enabled=%d\nuser=%s\npass=%s\n", enabled, user, pass);
    fclose(f);
}

bool wfview_server_get_enabled(void) {
    return enabled;
}

void wfview_server_set_enabled(bool on) {
    enabled = on;
    if (!on)
        disconnect_all();
    save_conf();
}

void wfview_server_set_credentials(const char *u, const char *p) {
    snprintf(user, sizeof(user), "%s", u);
    snprintf(pass, sizeof(pass), "%s", p);
    save_conf();
}

const char *wfview_server_user(void) {
    return user;
}

const char *wfview_server_pass(void) {
    return pass;
}

const char *wfview_server_client(void) {
    return enabled && logged && ports[P_CTRL].connected ? client_name : NULL;
}

/* Icom LAN password encoding; exposed for the tests and the web configurator */
void wfview_server_passcode(const char *in, uint8_t out[16]) {
    passcode(in, out);
}

void wfview_server_init() {
    pthread_t th;

    load_conf();
    srand(time(NULL));
    for (int i = 0; i < P_NUM; i++) {
        port_t *pt = &ports[i];
        pt->port   = 50001 + i;
        pt->my_id  = rand() | 1;
        pt->fd     = socket(AF_INET, SOCK_DGRAM, 0);
        pthread_mutex_init(&pt->lock, NULL);
        struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(pt->port), .sin_addr.s_addr = INADDR_ANY};
        if (bind(pt->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            LV_LOG_ERROR("wfview: bind %u failed", pt->port);
            return;
        }
    }
    cat_set_net_sink(send_civ);
    pthread_create(&th, NULL, net_thread, NULL);
    pthread_detach(th);
    pthread_create(&th, NULL, audio_thread, NULL);
    pthread_detach(th);
}

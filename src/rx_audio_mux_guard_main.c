/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  A-prime (#367): helper guardian independiente de la GUI.
 *
 *  INACTIVO por defecto: arranca escuchando en un socket UNIX, pero NO acepta armarse
 *  sin un handshake valido con readback real confirmado, y NO escribe el registro 12
 *  mientras no exista una lectura real del MCU (Fase R = R_BLOCKED_NO_HARDWARE_TRUTH).
 *  Por eso el supervisor x6200_daemon NO lo lanza en produccion todavia: el binario se
 *  construye/instala con el build real y se prueba con mocks (host) y ARM (cross), pero
 *  la ruta activa queda OFF hasta que #363 vuelva a A_PASS.
 *
 *  Uso: x6200_audio_guard [--socket /run/x6200_audio_guard.sock] [--self-check]
 */

#define _GNU_SOURCE

#include "rx_audio_mux_guard.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define RXMUX_GUARD_SOCK_DEFAULT "/run/x6200_audio_guard.sock"

/* En produccion NO hay readback real del registro 12: el helper nunca escribe.
 * (Ver readback-re.md; el guardian solo se probo con bus mock en host.) */
#define RXMUX_HELPER_READBACK_AVAILABLE 0

static volatile sig_atomic_t stop_requested = 0;
static const char *socket_path = RXMUX_GUARD_SOCK_DEFAULT;

static void on_signal(int sig) {
    (void) sig;
    stop_requested = 1;
}

static void log_line(const char *what) {
    fprintf(stderr, "x6200_audio_guard: %s\n", what);
}

static int send_msg(int fd, const rxmux_ipc_msg_t *msg) {
    uint8_t buf[sizeof(rxmux_ipc_msg_t)];
    size_t n = rxmux_ipc_encode(msg, buf, sizeof(buf));
    if (n != sizeof(buf)) {
        return -1;
    }
    ssize_t w = send(fd, buf, n, 0);
    return (w == (ssize_t) n) ? 0 : -1;
}

static int recv_msg(int fd, rxmux_ipc_msg_t *msg) {
    uint8_t buf[sizeof(rxmux_ipc_msg_t)];
    ssize_t r = recv(fd, buf, sizeof(buf), 0);
    if (r != (ssize_t) sizeof(buf)) {
        return -1;
    }
    return rxmux_ipc_decode(buf, (size_t) r, msg);
}

/* Respuesta STATUS/ARM_ACK: el ARM se rechaza porque no hay readback real. */
static void reply_status(int fd, const rxmux_ipc_msg_t *req, uint32_t flags) {
    rxmux_ipc_msg_t out;
    memset(&out, 0, sizeof(out));
    out.magic = RXMUX_IPC_MAGIC;
    out.version = RXMUX_IPC_VERSION;
    out.type = RXMUX_MSG_ARM_ACK;
    memcpy(out.token, req->token, RXMUX_TOKEN_LEN);
    out.pid = req->pid;
    out.starttime = req->starttime;
    out.mono_ms = rxmux_now_ms();
    out.seq = req->seq + 1u;
    out.flags = flags;
    (void) send_msg(fd, &out);
}

static int self_check(void) {
    /* Suma de invariantes que la evidencia G0/G1 necesita: la mascara preserva todo
     * menos 0x8 y el guardian sin readback no habilita escritura. */
    bool ok = true;
    ok = ok && (rxmux_preserve_mask(0xFFFFFFFFu, false) == (0xFFFFFFFFu & ~RXMUX_BIT_VOICE_REC));
    ok = ok && (rxmux_preserve_mask(0x00040000u, true) == 0x00040008u);
    ok = ok && rxmux_has_tx_activity(RXMUX_BIT_IPTT);
    ok = ok && rxmux_has_tx_activity(RXMUX_BIT_ATU_TUNE);
    ok = ok && !rxmux_has_tx_activity(RXMUX_BIT_VOICE_REC);
    rxmux_ipc_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.magic = RXMUX_IPC_MAGIC;
    msg.version = RXMUX_IPC_VERSION;
    msg.type = RXMUX_MSG_ARM;
    msg.pid = 1;
    msg.starttime = 1;
    msg.mono_ms = 1;
    msg.lease_ms = RXMUX_LEASE_TIMEOUT_MS;
    rxmux_guardian_t g;
    rxmux_guardian_init(&g);
    ok = ok && (rxmux_guardian_arm(&g, &msg, 1) == RXMUX_BLOCKED_NO_READBACK);
    ok = ok && (RXMUX_HELPER_READBACK_AVAILABLE == 0);
    if (ok) {
        printf("x6200_audio_guard self-check ok (readback=%s, guardian=%s)\n",
               "UNAVAILABLE", "OFF");
        return 0;
    }
    printf("x6200_audio_guard self-check FAIL\n");
    return 1;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--self-check") == 0) {
            return self_check();
        } else if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            socket_path = argv[++i];
        } else {
            fprintf(stderr, "uso: %s [--socket PATH] [--self-check]\n", argv[0]);
            return 2;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    int srv = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (srv < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(socket_path) >= sizeof(addr.sun_path)) {
        fprintf(stderr, "socket path demasiado largo\n");
        close(srv);
        return 1;
    }
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    unlink(socket_path);
    if (bind(srv, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        perror("bind");
        close(srv);
        return 1;
    }
    chmod(socket_path, 0600);
    if (listen(srv, 4) != 0) {
        perror("listen");
        close(srv);
        unlink(socket_path);
        return 1;
    }

    if (RXMUX_HELPER_READBACK_AVAILABLE == 0) {
        log_line("started INACTIVE: no verified reg12 readback (R_BLOCKED_NO_HARDWARE_TRUTH); "
                 "guardian will refuse arm and will never write reg12");
    }

    rxmux_guardian_t guardian;
    rxmux_guardian_init(&guardian);
    uint64_t last_tick = rxmux_now_ms();

    while (!stop_requested) {
        struct timeval tv = { .tv_sec = 0, .tv_usec = 5000 };
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);
        int rc = select(srv + 1, &rfds, NULL, NULL, &tv);
        if (rc > 0 && FD_ISSET(srv, &rfds)) {
            int fd = accept4(srv, NULL, NULL, SOCK_CLOEXEC);
            if (fd >= 0) {
                rxmux_ipc_msg_t msg;
                while (!stop_requested && recv_msg(fd, &msg) == 0) {
                    switch (msg.type) {
                        case RXMUX_MSG_ARM:
                            if (msg.flags & RXMUX_FLAG_READBACK_CONFIRMED) {
                                /* En produccion no hay proveedor real de readback. */
                                reply_status(fd, &msg, 0);
                                log_line("ARM refused: no readback provider");
                            } else {
                                reply_status(fd, &msg, 0);
                                log_line("ARM refused: missing READBACK_CONFIRMED");
                            }
                            break;
                        case RXMUX_MSG_HEARTBEAT:
                            (void) rxmux_guardian_heartbeat(&guardian, &msg,
                                                            rxmux_now_ms());
                            break;
                        case RXMUX_MSG_DISARM:
                            rxmux_guardian_init(&guardian);
                            break;
                        default:
                            break;
                    }
                }
                close(fd);
            }
        }
        uint64_t now = rxmux_now_ms();
        if (now - last_tick >= 1u) {
            last_tick = now;
            rxmux_guard_action_t action = rxmux_guardian_tick(&guardian, now);
            if (action == RXMUX_GUARD_REFUSE_NO_READBACK) {
                log_line("lease expired but no readback: fail-closed, no write");
            }
        }
    }

    close(srv);
    unlink(socket_path);
    log_line("stopped (reg12 untouched)");
    return 0;
}

/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  A-prime (#367) — tests nativos del coordinador de reg 12 y del guardian fail-safe.
 *
 *  G1: mascara segura (>=2009 valores), 1000 eventos interleaved, dac=true jamas con
 *      TX/ATU/modem, readback mismatch nunca arma.
 *  G2: worker + guardian como PROCESOS REALES (fork + socketpair + bus mock en mmap
 *      compartido), 100 repeticiones, restore total medido con CLOCK_MONOTONIC desde
 *      el ultimo heartbeat valido (objetivo <=250 ms), con casos de GUI responsiva y
 *      de GUI bloqueada/detenida (SIGKILL + verificacion de que el bus quedo libre).
 *  G3: lifecycle del helper x6200_audio_guard (arranca INACTIVO, rechaza ARM sin
 *      readback, SIGTERM limpia el socket) si RXMUX_GUARD_BIN esta definido.
 *
 *  NO toca hardware: el bus es mock (memoria compartida). Host-only: no es PASS de
 *  hardware (ver gates.csv).
 */

#define _GNU_SOURCE

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "rx_audio_mux_guard.h"
#include "test_framework.h"

/* --- Bus mock compartido entre procesos ------------------------------------ */

typedef struct {
    pthread_mutex_t lock;
    uint32_t        reg;          /* valor real del registro 12 */
    uint32_t        writes;
    uint32_t        reads;
    pid_t           owner;        /* pid del escritor que lo tiene tomado, 0 libre */
    int             corrupt_next_reads;
} mockbus_t;

static mockbus_t *bus;

static void bus_init(void) {
    bus = mmap(NULL, sizeof(*bus), PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (bus == MAP_FAILED) {
        perror("mmap");
        exit(2);
    }
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&bus->lock, &attr);
    pthread_mutexattr_destroy(&attr);
    bus->reg = 0;
    bus->writes = 0;
    bus->reads = 0;
    bus->owner = 0;
    bus->corrupt_next_reads = 0;
}

static int mock_read(void *ctx, uint32_t *out) {
    (void) ctx;
    pthread_mutex_lock(&bus->lock);
    uint32_t v = bus->reg;
    bus->reads++;
    if (bus->corrupt_next_reads > 0) {
        bus->corrupt_next_reads--;
        v ^= 0x00000100u;   /* corrompe un bit distinto de 0x8 */
    }
    pthread_mutex_unlock(&bus->lock);
    *out = v;
    return 0;
}

static int mock_write(void *ctx, uint32_t value) {
    (void) ctx;
    pthread_mutex_lock(&bus->lock);
    bus->reg = value;
    bus->writes++;
    pthread_mutex_unlock(&bus->lock);
    return 0;
}

static int mock_read_fail(void *ctx, uint32_t *out) {
    (void) ctx;
    (void) out;
    return -1;
}

static int mock_write_fail(void *ctx, uint32_t value) {
    (void) ctx;
    (void) value;
    return -1;
}

static void bus_set(uint32_t value) {
    pthread_mutex_lock(&bus->lock);
    bus->reg = value;
    pthread_mutex_unlock(&bus->lock);
}

static uint32_t bus_get(void) {
    pthread_mutex_lock(&bus->lock);
    uint32_t v = bus->reg;
    pthread_mutex_unlock(&bus->lock);
    return v;
}

static const rxmux_bus_ops_t MOCK_OPS = {
    .read_reg12 = mock_read,
    .write_reg12 = mock_write,
    .ctx = NULL,
};

/* --- G1: mascara + invariantes -------------------------------------------- */

static uint32_t lcg_state = 6200u;
static uint32_t lcg_next(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

void test_rxmux_mask_preserves(void) {
    const uint32_t fixed[] = {
        0x0u, 0xFFFFFFFFu, RXMUX_BIT_VOICE_REC, RXMUX_BIT_IPTT,
        RXMUX_BIT_ATUE | RXMUX_BIT_ATU_TUNE, RXMUX_BIT_MODEM,
        RXMUX_BIT_SWRSCAN, RXMUX_BIT_POWER_OFF,
        RXMUX_BIT_IPTT | RXMUX_BIT_VOICE_REC, 0x00001002u, 0x00040002u,
    };
    const size_t nfixed = sizeof(fixed) / sizeof(fixed[0]);

    for (size_t i = 0; i < 2009; i++) {
        uint32_t v = (i < nfixed) ? fixed[i] : lcg_next();
        uint32_t next = 0;

        CHECK(rxmux_preserve_mask(v, false) == (v & ~RXMUX_BIT_VOICE_REC));
        CHECK(rxmux_preserve_mask(v, true) == (v | RXMUX_BIT_VOICE_REC));

        rxmux_result_t r = rxmux_plan(v, false, &next);
        if (rxmux_has_tx_activity(v)) {
            CHECK(r == RXMUX_BLOCKED_TX_ACTIVE);
        } else {
            CHECK(r == RXMUX_OK);
            CHECK(next == (v & ~RXMUX_BIT_VOICE_REC));
            CHECK((next & RXMUX_PRESERVE_MASK) == (v & RXMUX_PRESERVE_MASK));
        }

        rxmux_result_t r2 = rxmux_plan(v, true, &next);
        if (rxmux_has_tx_activity(v)) {
            CHECK(r2 == RXMUX_BLOCKED_TX_ACTIVE);
        } else {
            CHECK(r2 == RXMUX_OK);
            CHECK(next == (v | RXMUX_BIT_VOICE_REC));
            CHECK(!rxmux_has_tx_activity(next));
            CHECK((next & RXMUX_PRESERVE_MASK) == (v & RXMUX_PRESERVE_MASK));
        }
    }

    CHECK(rxmux_plan(0, false, NULL) == RXMUX_BLOCKED_INVALID);
    CHECK(rxmux_foreign_voice_rec(0, false) == false);
    CHECK(rxmux_foreign_voice_rec(RXMUX_BIT_VOICE_REC, false) == true);
    CHECK(rxmux_foreign_voice_rec(RXMUX_BIT_VOICE_REC, true) == false);
}

/* 1000 eventos interleaved: idle (bloque completo desde sombra), PTT, ATU, record,
 * cambio RX/TX. El coordinador nunca debe cambiar bits fuera de 0x8 y nunca debe
 * escribir con TX/ATU/modem activos. */
void test_rxmux_interleaved_1000(void) {
    bus_init();
    uint32_t shadow = 0;
    int ok_writes = 0;
    int blocked = 0;

    bus_set(0);
    for (int i = 0; i < 1000; i++) {
        uint32_t ev = lcg_next() % 6u;
        bool coordinator_owns = false;
        switch (ev) {
            case 0:   /* GUI idle: reenvia el bloque desde la sombra */
                if ((shadow & RXMUX_BIT_IPTT) == 0) {
                    shadow = rxmux_preserve_mask(shadow, false);
                }
                bus_set(shadow);
                continue;
            case 1:   /* PTT on/off (RMW de la lib sobre la sombra) */
                shadow ^= RXMUX_BIT_IPTT;
                continue;
            case 2:   /* ATU enable */
                shadow ^= RXMUX_BIT_ATUE;
                continue;
            case 3:   /* audio generado/record: otro camino prende 0x8 (sombra) */
                shadow |= RXMUX_BIT_VOICE_REC;
                continue;
            case 4:   /* coordinador pide DAC */
            case 5:   /* coordinador vuelve a BASE */
                coordinator_owns = true;
                break;
            default:
                break;
        }

        uint32_t before = bus_get();
        (void) coordinator_owns;
        rxmux_result_t r = rxmux_coordinator_apply(&MOCK_OPS, ev == 4, NULL);
        uint32_t after = bus_get();
        if (r == RXMUX_OK) {
            ok_writes++;
            CHECK((after & RXMUX_PRESERVE_MASK) == (before & RXMUX_PRESERVE_MASK));
            CHECK((after & RXMUX_BIT_VOICE_REC) == (ev == 4 ? RXMUX_BIT_VOICE_REC : 0u));
            if (ev == 4) {
                CHECK(!rxmux_has_tx_activity(before));
            }
        } else {
            blocked++;
            CHECK(after == before);
        }
    }
    /* Con un bus ideal (readback real mock) y eventos que alternan TX, ambos caminos
     * tienen que aparecer: si todo se bloquea o todo escribe, la simulacion no cubre
     * los interlocks. */
    CHECK(ok_writes > 0);
    CHECK(blocked > 0);
}

void test_rxmux_coordinator_mock(void) {
    bus_init();
    bus_set(0x00001002u);            /* ATUE + split: interlock de TX/ATU */
    uint32_t w = bus->writes;
    CHECK(rxmux_coordinator_apply(&MOCK_OPS, true, NULL) == RXMUX_BLOCKED_TX_ACTIVE);
    CHECK(bus->writes == w);

    bus_set(0x00000002u);            /* split, sin TX/ATU */
    CHECK(rxmux_coordinator_apply(&MOCK_OPS, true, NULL) == RXMUX_OK);
    CHECK(bus_get() == 0x0000000Au);
    uint32_t w_after = bus->writes;
    CHECK(rxmux_coordinator_apply(&MOCK_OPS, true, NULL) == RXMUX_OK);   /* idempotente */
    CHECK(bus->writes == w_after);   /* no reescribe si ya esta */
    CHECK(rxmux_coordinator_apply(&MOCK_OPS, false, NULL) == RXMUX_OK);
    CHECK(bus_get() == 0x00000002u);

    /* readback mismatch: el write ocurre pero la verificacion no coincide -> VERIFY */
    bus_set(0x00000000u);
    bus->corrupt_next_reads = 2;     /* corrompe el read previo y el de verificacion */
    CHECK(rxmux_coordinator_apply(&MOCK_OPS, true, NULL) == RXMUX_BLOCKED_VERIFY);

    bus_set(0x00000000u);
    const rxmux_bus_ops_t fail_read = { mock_read_fail, mock_write, NULL };
    const rxmux_bus_ops_t fail_write = { mock_read, mock_write_fail, NULL };
    CHECK(rxmux_coordinator_apply(&fail_read, true, NULL) == RXMUX_BLOCKED_BUS);
    CHECK(rxmux_coordinator_apply(&fail_write, true, NULL) == RXMUX_BLOCKED_BUS);
    CHECK(rxmux_coordinator_apply(NULL, true, NULL) == RXMUX_BLOCKED_NO_READBACK);
}

void test_rxmux_restore_mock(void) {
    bus_init();
    bus_set(0x00040002u | RXMUX_BIT_VOICE_REC);   /* PTT + split + voice_rec */
    uint32_t w = bus->writes;
    CHECK(rxmux_restore_apply(&MOCK_OPS, NULL) == RXMUX_OK);
    CHECK(bus_get() == 0x00040002u);              /* preserva PTT y split */
    CHECK(bus->writes == w + 1);

    w = bus->writes;
    CHECK(rxmux_restore_apply(&MOCK_OPS, NULL) == RXMUX_OK);  /* ya en BASE */
    CHECK(bus->writes == w);

    CHECK(rxmux_restore_apply(NULL, NULL) == RXMUX_BLOCKED_NO_READBACK);
}

/* --- G1/G3: protocolo e handshake ----------------------------------------- */

static void make_msg(rxmux_ipc_msg_t *m, uint16_t type, const uint8_t token[16],
                     uint32_t pid, uint32_t starttime, uint64_t now, uint32_t seq,
                     uint32_t flags) {
    memset(m, 0, sizeof(*m));
    m->magic = RXMUX_IPC_MAGIC;
    m->version = RXMUX_IPC_VERSION;
    m->type = type;
    memcpy(m->token, token, RXMUX_TOKEN_LEN);
    m->pid = pid;
    m->starttime = starttime;
    m->mono_ms = now;
    m->seq = seq;
    m->flags = flags;
    m->lease_ms = RXMUX_LEASE_TIMEOUT_MS;
}

void test_rxmux_ipc(void) {
    uint8_t token[RXMUX_TOKEN_LEN] = { 't', 'o', 'k', 'e', 'n', 1, 2, 3 };
    rxmux_ipc_msg_t in;
    make_msg(&in, RXMUX_MSG_ARM, token, 1234, 5678, 1000, 1,
             RXMUX_FLAG_READBACK_CONFIRMED);
    in.base_reg12 = 0x00000002u;

    uint8_t buf[sizeof(rxmux_ipc_msg_t) + 8];
    size_t n = rxmux_ipc_encode(&in, buf, sizeof(buf));
    CHECK(n == sizeof(rxmux_ipc_msg_t));
    rxmux_ipc_msg_t out;
    CHECK(rxmux_ipc_decode(buf, n, &out) == 0);
    CHECK(memcmp(&in, &out, sizeof(in)) == 0);

    CHECK(rxmux_ipc_decode(buf, n - 1, &out) != 0);
    buf[0] ^= 0xFF;
    CHECK(rxmux_ipc_decode(buf, n, &out) != 0);
    CHECK(rxmux_ipc_decode(buf, n, NULL) != 0);

    CHECK(rxmux_arm_validate(&in, token, 1234, 5678) == RXMUX_OK);
    CHECK(rxmux_arm_validate(&in, token, 9999, 5678) == RXMUX_BLOCKED_INVALID);
    CHECK(rxmux_arm_validate(&in, token, 1234, 9999) == RXMUX_BLOCKED_INVALID);
    uint8_t bad_token[RXMUX_TOKEN_LEN] = { 0 };
    CHECK(rxmux_arm_validate(&in, bad_token, 1234, 5678) == RXMUX_BLOCKED_INVALID);

    rxmux_ipc_msg_t no_readback = in;
    no_readback.flags = 0;
    CHECK(rxmux_arm_validate(&no_readback, token, 1234, 5678) ==
          RXMUX_BLOCKED_NO_READBACK);

    rxmux_ipc_msg_t big_lease = in;
    big_lease.lease_ms = RXMUX_LEASE_TIMEOUT_MS + 1;
    CHECK(rxmux_arm_validate(&big_lease, token, 1234, 5678) == RXMUX_BLOCKED_INVALID);

    rxmux_ipc_msg_t wrong_type = in;
    wrong_type.type = RXMUX_MSG_HEARTBEAT;
    CHECK(rxmux_arm_validate(&wrong_type, token, 1234, 5678) == RXMUX_BLOCKED_INVALID);
}

void test_rxmux_guardian_fsm(void) {
    uint8_t token[RXMUX_TOKEN_LEN] = { 'a', 'b', 'c', 9, 8, 7 };
    rxmux_ipc_msg_t arm;
    make_msg(&arm, RXMUX_MSG_ARM, token, 42, 99, 1000, 1,
             RXMUX_FLAG_READBACK_CONFIRMED);

    rxmux_guardian_t g;
    rxmux_guardian_init(&g);
    CHECK(rxmux_guardian_arm(&g, &arm, 1000) == RXMUX_OK);
    CHECK(g.armed);

    rxmux_ipc_msg_t hb;
    make_msg(&hb, RXMUX_MSG_HEARTBEAT, token, 42, 99, 1020, 2, 0);
    CHECK(rxmux_guardian_heartbeat(&g, &hb, 1020) == RXMUX_GUARD_NONE);
    CHECK(rxmux_guardian_tick(&g, 1120) == RXMUX_GUARD_NONE);      /* <=100ms */
    CHECK(rxmux_guardian_tick(&g, 1121) == RXMUX_GUARD_REQUEST_RESTORE);
    CHECK(rxmux_guardian_tick(&g, 1150) == RXMUX_GUARD_STOP_WRITER);
    rxmux_guardian_mark_writer_stopped(&g);
    CHECK(rxmux_guardian_tick(&g, 1151) == RXMUX_GUARD_RESTORE_NOW);

    /* Heartbeat de otra instancia no renueva el lease. */
    rxmux_guardian_init(&g);
    CHECK(rxmux_guardian_arm(&g, &arm, 1000) == RXMUX_OK);
    rxmux_ipc_msg_t wrong = hb;
    memcpy(wrong.token, token, RXMUX_TOKEN_LEN);
    wrong.token[0] ^= 0xFF;
    CHECK(rxmux_guardian_heartbeat(&g, &wrong, 1050) == RXMUX_GUARD_NONE);
    CHECK(rxmux_guardian_tick(&g, 1121) == RXMUX_GUARD_REQUEST_RESTORE);

    /* Arm sin readback confirmado: rechazado, guardian nunca arma. */
    rxmux_ipc_msg_t no_rb;
    make_msg(&no_rb, RXMUX_MSG_ARM, token, 42, 99, 1000, 1, 0);
    rxmux_guardian_init(&g);
    CHECK(rxmux_guardian_arm(&g, &no_rb, 1000) == RXMUX_BLOCKED_NO_READBACK);
    CHECK(!g.armed);

    /* Sin readback el tick vencido se identifica y NO habilita escritura. */
    rxmux_guardian_init(&g);
    CHECK(rxmux_guardian_arm(&g, &arm, 1000) == RXMUX_OK);
    g.readback_confirmed = false;
    CHECK(rxmux_guardian_tick(&g, 1200) == RXMUX_GUARD_REFUSE_NO_READBACK);
}

/* --- G2: procesos reales, 100 repeticiones, restore medido ----------------- */

#define RXMUX_BASE_PRESERVE 0x00040002u   /* PTT + split: el restore debe preservarlos */
#define RXMUX_REPS 100
#define RXMUX_HB_PERIOD_MS 10
#define RXMUX_HB_COUNT 4

typedef struct {
    double   samples[RXMUX_REPS];
    uint32_t n;
} latency_stats_t;

static int msg_send(int fd, const rxmux_ipc_msg_t *m) {
    uint8_t buf[sizeof(rxmux_ipc_msg_t)];
    size_t n = rxmux_ipc_encode(m, buf, sizeof(buf));
    if (n != sizeof(buf)) {
        return -1;
    }
    return (send(fd, buf, n, 0) == (ssize_t) n) ? 0 : -1;
}

static int msg_recv_timeout(int fd, rxmux_ipc_msg_t *m, uint64_t timeout_ms) {
    struct pollfd p = { .fd = fd, .events = POLLIN };
    uint64_t deadline = rxmux_now_ms() + timeout_ms;
    for (;;) {
        uint64_t now = rxmux_now_ms();
        if (now >= deadline) {
            return -1;
        }
        int rc = poll(&p, 1, (int) (deadline - now));
        if (rc > 0 && (p.revents & POLLIN)) {
            uint8_t buf[sizeof(rxmux_ipc_msg_t)];
            ssize_t r = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
            if (r == (ssize_t) sizeof(buf)) {
                return rxmux_ipc_decode(buf, (size_t) r, m);
            }
            return -1;
        }
    }
}

static double now_ms_d(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1e6;
}

/* Hijo: escritor simulado (coordinador de la GUI). Toma el bus, prende voice_rec,
 * publica heartbeats y despues (a) responde el restore si es "responsiva" o (b) se
 * bloquea, para forzar el camino de STOP_WRITER + restore del guardian. */
static void worker_child(int fd, bool responsive, uint8_t token[RXMUX_TOKEN_LEN]) {
    pthread_mutex_lock(&bus->lock);
    bus->reg = RXMUX_BASE_PRESERVE | RXMUX_BIT_VOICE_REC;
    bus->owner = getpid();
    pthread_mutex_unlock(&bus->lock);

    for (uint32_t i = 1; i <= RXMUX_HB_COUNT; i++) {
        rxmux_ipc_msg_t hb;
        make_msg(&hb, RXMUX_MSG_HEARTBEAT, token, getpid(), 1,
                 rxmux_now_ms(), i, 0);
        (void) msg_send(fd, &hb);
        struct timespec ts = { .tv_sec = 0, .tv_nsec = RXMUX_HB_PERIOD_MS * 1000000L };
        nanosleep(&ts, NULL);
    }

    if (!responsive) {
        for (;;) {                      /* GUI bloqueada: no atiende el socket */
            struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
            nanosleep(&ts, NULL);
        }
    }

    rxmux_ipc_msg_t req;
    if (msg_recv_timeout(fd, &req, 500) == 0 && req.type == RXMUX_MSG_RESTORE_REQ) {
        const rxmux_bus_ops_t ops = MOCK_OPS;
        (void) rxmux_restore_apply(&ops, NULL);
        rxmux_ipc_msg_t ack;
        make_msg(&ack, RXMUX_MSG_RESTORE_ACK, token, getpid(), 1,
                 rxmux_now_ms(), req.seq + 1, 0);
        (void) msg_send(fd, &ack);
    }
    _exit(0);
}

static bool parent_guardian_loop(int fd, pid_t child, uint8_t token[RXMUX_TOKEN_LEN],
                                 bool responsive, double *out_latency_ms) {
    rxmux_guardian_t g;
    rxmux_guardian_init(&g);

    rxmux_ipc_msg_t arm;
    make_msg(&arm, RXMUX_MSG_ARM, token, child, 1, rxmux_now_ms(), 0,
             RXMUX_FLAG_READBACK_CONFIRMED);
    if (rxmux_guardian_arm(&g, &arm, rxmux_now_ms()) != RXMUX_OK) {
        return false;
    }

    double last_hb = now_ms_d();
    uint64_t restore_sent = 0;
    bool writer_stopped = false;

    for (;;) {
        rxmux_ipc_msg_t msg;
        if (msg_recv_timeout(fd, &msg, 1) == 0) {
            if (msg.type == RXMUX_MSG_HEARTBEAT && msg.seq > g.last_seq) {
                CHECK(rxmux_guardian_heartbeat(&g, &msg, rxmux_now_ms()) ==
                      RXMUX_GUARD_NONE);
                g.last_seq = msg.seq;
                last_hb = now_ms_d();
            } else if (msg.type == RXMUX_MSG_RESTORE_ACK) {
                break;   /* el coordinador restauro; el guardian verifica readback */
            }
        }

        uint64_t now = rxmux_now_ms();
        rxmux_guard_action_t action = rxmux_guardian_tick(&g, now);
        if (action == RXMUX_GUARD_REQUEST_RESTORE && restore_sent == 0) {
            rxmux_ipc_msg_t req;
            make_msg(&req, RXMUX_MSG_RESTORE_REQ, token, child, 1, now, g.last_seq + 1, 0);
            (void) msg_send(fd, &req);
            restore_sent = now;
        } else if (action == RXMUX_GUARD_STOP_WRITER && !writer_stopped) {
            kill(child, SIGKILL);
            (void) waitpid(child, NULL, 0);
            writer_stopped = true;
            rxmux_guardian_mark_writer_stopped(&g);
            /* Verificacion de que el unico escritor cerro el bus: el owner quedo muerto. */
            pthread_mutex_lock(&bus->lock);
            bool bus_free = (bus->owner == 0 || kill(bus->owner, 0) != 0);
            pthread_mutex_unlock(&bus->lock);
            CHECK(bus_free);
            if (!bus_free) {
                return false;
            }
        } else if (action == RXMUX_GUARD_RESTORE_NOW && writer_stopped) {
            const rxmux_bus_ops_t ops = MOCK_OPS;
            if (rxmux_restore_apply(&ops, NULL) != RXMUX_OK) {
                return false;
            }
            break;
        }

        if (now_ms_d() - last_hb > (double) RXMUX_RESTORE_BUDGET_MS + 200.0) {
            return false;   /* watchdog del harness: nunca colgar el test */
        }
    }

    /* El guardian NUNCA asume: verifica el registro real por readback. */
    if (bus_get() != RXMUX_BASE_PRESERVE) {
        return false;
    }
    *out_latency_ms = now_ms_d() - last_hb;
    if (responsive) {
        int status = 0;
        if (waitpid(child, &status, 0) != child) {
            return false;
        }
    }
    (void) writer_stopped;
    return true;
}

static bool run_rep(bool responsive, uint8_t token[RXMUX_TOKEN_LEN], double *latency) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) != 0) {
        return false;
    }
    /* bus es MAP_SHARED: se resetea antes de cada fork. */
    pthread_mutex_lock(&bus->lock);
    bus->reg = 0;
    bus->owner = 0;
    bus->writes = 0;
    bus->reads = 0;
    bus->corrupt_next_reads = 0;
    pthread_mutex_unlock(&bus->lock);

    pid_t child = fork();
    if (child < 0) {
        return false;
    }
    if (child == 0) {
        close(sv[0]);
        worker_child(sv[1], responsive, token);
        _exit(0);
    }
    close(sv[1]);

    struct timespec settle = { .tv_sec = 0, .tv_nsec = 2000000L };
    nanosleep(&settle, NULL);  /* deja al hijo prender voice_rec antes del primer tick */

    bool ok = parent_guardian_loop(sv[0], child, token, responsive, latency);
    close(sv[0]);
    if (responsive) {
        (void) waitpid(child, NULL, 0);
    }
    return ok;
}

void test_rxmux_process_recovery(void) {
    bus_init();
    uint8_t token[RXMUX_TOKEN_LEN] = { 0x36, 0x37, 0x67, 0xAA };
    double lat[RXMUX_REPS];
    int lat_n = 0;
    int ok_count = 0;

    for (int i = 0; i < RXMUX_REPS; i++) {
        bool responsive = (i % 2) == 0;
        double l = 0;
        if (run_rep(responsive, token, &l)) {
            ok_count++;
            lat[lat_n++] = l;
        }
    }
    CHECK(ok_count == RXMUX_REPS);
    CHECK(lat_n == RXMUX_REPS);

    /* p50/p99/max (el assert duro es el max: objetivo del issue <=250 ms). */
    for (int i = 0; i < lat_n; i++) {
        for (int j = i + 1; j < lat_n; j++) {
            if (lat[j] < lat[i]) {
                double t = lat[i];
                lat[i] = lat[j];
                lat[j] = t;
            }
        }
    }
    double p50 = lat_n > 0 ? lat[lat_n / 2] : 0;
    double p99 = lat_n > 0 ? lat[(lat_n * 99) / 100] : 0;
    double max = lat_n > 0 ? lat[lat_n - 1] : 0;
    printf("      G2 restore: %d repeticiones, p50=%.1f ms, p99=%.1f ms, max=%.1f ms "
           "(objetivo <= %u ms)\n", lat_n, p50, p99, max, RXMUX_RESTORE_BUDGET_MS);
    CHECK(max <= (double) RXMUX_RESTORE_BUDGET_MS);
    CHECK(p99 <= (double) RXMUX_RESTORE_BUDGET_MS);
    CHECK(p50 <= (double) RXMUX_RESTORE_BUDGET_MS);
}

/* --- G3: lifecycle del helper real --------------------------------------- */

void test_rxmux_helper_lifecycle(void) {
    const char *bin = getenv("RXMUX_GUARD_BIN");
    if (bin == NULL || bin[0] == '\0') {
        printf("      G3 helper lifecycle: SKIP (RXMUX_GUARD_BIN no definido)\n");
        return;
    }

    bus_set(0);
    char path[128];
    snprintf(path, sizeof(path), "/tmp/rxmux_guard_test_%ld.sock", (long) getpid());
    unlink(path);

    pid_t pid = fork();
    if (pid == 0) {
        execl(bin, bin, "--socket", path, (char *) NULL);
        _exit(127);
    }
    CHECK(pid > 0);

    /* espera el socket */
    bool up = false;
    for (int i = 0; i < 200; i++) {
        if (access(path, F_OK) == 0) {
            up = true;
            break;
        }
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 5000000L };
        nanosleep(&ts, NULL);
    }
    CHECK(up);

    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    bool connected = (fd >= 0 && connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == 0);
    CHECK(connected);

    if (connected) {
        uint8_t token[RXMUX_TOKEN_LEN] = { 1 };
        rxmux_ipc_msg_t arm;
        make_msg(&arm, RXMUX_MSG_ARM, token, pid, 1, rxmux_now_ms(), 1,
                 RXMUX_FLAG_READBACK_CONFIRMED);
        CHECK(msg_send(fd, &arm) == 0);

        rxmux_ipc_msg_t ack;
        CHECK(msg_recv_timeout(fd, &ack, 1000) == 0);
        CHECK(ack.type == RXMUX_MSG_ARM_ACK);
        /* El helper NO puede armar sin readback real: rechaza el ARM. */
        CHECK(ack.flags == 0);
        CHECK(bus_get() == 0);   /* y no escribio ningun registro */
        close(fd);
    }

    kill(pid, SIGTERM);
    int status = 0;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(access(path, F_OK) != 0);   /* el socket se limpio */
    unlink(path);
}

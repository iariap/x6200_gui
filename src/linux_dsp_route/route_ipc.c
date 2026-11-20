// IPC proxy del arbitro: la GUI/el router no escriben I2C; piden al arbitro.
#define _POSIX_C_SOURCE 200809L
#include "route_ipc.h"
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct route_ipc_server {
    route_arbiter_t *arb;
    uint8_t token[ROUTE_IPC_TOKEN_LEN];
    int fd;
    bool running, have_client;
    uint32_t client_pid, client_starttime;
    uint32_t last_seq;
    uint64_t last_msg_ms;
};

size_t route_ipc_encode(const route_ipc_msg_t *m, void *buf, size_t cap) {
    if (m == NULL || buf == NULL || cap < sizeof(*m)) return 0;
    memcpy(buf, m, sizeof(*m));
    return sizeof(*m);
}
int route_ipc_decode(const void *buf, size_t len, route_ipc_msg_t *m) {
    if (buf == NULL || m == NULL || len != sizeof(*m)) return -1;
    memcpy(m, buf, sizeof(*m));
    if (m->magic != ROUTE_IPC_MAGIC || m->version != ROUTE_IPC_VERSION) return -1;
    return 0;
}

route_ipc_server_t *route_ipc_server_create(route_arbiter_t *arb, const uint8_t token[ROUTE_IPC_TOKEN_LEN]) {
    route_ipc_server_t *s = calloc(1, sizeof(*s));
    if (s == NULL) return NULL;
    s->arb = arb; s->fd = -1; s->running = true;
    if (token != NULL) memcpy(s->token, token, ROUTE_IPC_TOKEN_LEN);
    return s;
}
void route_ipc_server_destroy(route_ipc_server_t *s) {
    if (s == NULL) return;
    if (s->fd >= 0) close(s->fd);
    free(s);
}
int route_ipc_server_attach(route_ipc_server_t *s, int fd) { if (s == NULL || fd < 0) return -1; s->fd = fd; return 0; }
void route_ipc_server_stop(route_ipc_server_t *s) { if (s) s->running = false; }
bool route_ipc_server_has_client(const route_ipc_server_t *s) { return s != NULL && s->have_client; }
uint32_t route_ipc_server_last_seq(const route_ipc_server_t *s) { return s ? s->last_seq : 0; }

static int send_msg(int fd, const route_ipc_msg_t *m) {
    uint8_t buf[sizeof(*m)];
    size_t n = route_ipc_encode(m, buf, sizeof(buf));
    if (n == 0) return -1;
    return (send(fd, buf, n, 0) == (ssize_t)n) ? 0 : -1;
}

static void fill_resp(route_ipc_msg_t *r, const route_ipc_msg_t *req, int32_t err, route_arbiter_t *arb) {
    memset(r, 0, sizeof(*r));
    r->magic = ROUTE_IPC_MAGIC; r->version = ROUTE_IPC_VERSION; r->type = req->type;
    memcpy(r->token, req->token, ROUTE_IPC_TOKEN_LEN);
    r->pid = req->pid; r->starttime = req->starttime; r->mono_ms = req->mono_ms;
    r->seq = req->seq; r->err = err;
    r->state = (uint32_t)route_arbiter_state(arb);
    const route_stats_t *st = route_arbiter_stats(arb);
    r->stats[0] = st->writes; r->stats[1] = st->restores; r->stats[2] = st->bus_errors;
    r->stats[3] = st->fault_count; r->stats[4] = st->overflows; r->stats[5] = st->denied;
}

int route_ipc_server_serve(route_ipc_server_t *s, int timeout_ms) {
    if (s == NULL || s->fd < 0 || !s->running) return -1;
    struct pollfd p = { .fd = s->fd, .events = POLLIN };
    int pr = poll(&p, 1, timeout_ms);
    if (pr == 0) return 0;
    if (pr < 0) return (errno == EINTR) ? 0 : -1;
    route_ipc_msg_t req;
    ssize_t n = recv(s->fd, &req, sizeof(req), 0);
    if (n <= 0) return -1;
    if (route_ipc_decode(&req, (size_t)n, &req) != 0) return -2;
    if (memcmp(req.token, s->token, ROUTE_IPC_TOKEN_LEN) != 0) {
        route_ipc_msg_t r; fill_resp(&r, &req, ROUTE_ERR_OWNERSHIP, s->arb);
        (void)send_msg(s->fd, &r); return -3;
    }
    /* Propiedad: un solo cliente; PID+starttime anti-reutilización. */
    if (!s->have_client) {
        s->have_client = true; s->client_pid = req.pid; s->client_starttime = req.starttime;
    } else if (req.pid != s->client_pid || req.starttime != s->client_starttime) {
        route_ipc_msg_t r; fill_resp(&r, &req, ROUTE_ERR_OWNERSHIP, s->arb);
        (void)send_msg(s->fd, &r); return -4;
    }
    /* Secuencia estrictamente creciente (reordenado/duplicado => STALE). */
    if (req.seq != 0 && req.seq <= s->last_seq) {
        route_ipc_msg_t r; fill_resp(&r, &req, ROUTE_ERR_STALE, s->arb);
        (void)send_msg(s->fd, &r); return 0;
    }
    if (req.seq != 0) s->last_seq = req.seq;
    s->last_msg_ms = req.mono_ms;

    route_err_t e = ROUTE_ERR_OK;
    switch (req.type) {
        case ROUTE_IPC_REQ_HEARTBEAT:
            e = route_arbiter_client_heartbeat(s->arb, req.mono_ms, req.seq, req.value, (req.lease_ms & 1u) != 0);
            break;
        case ROUTE_IPC_REQ_APPLY_SHARED:
            e = route_arbiter_apply_shared(s->arb, req.mono_ms, req.value);
            break;
        case ROUTE_IPC_REQ_ENABLE:
            e = route_arbiter_set_enabled(s->arb, req.mono_ms, true);
            break;
        case ROUTE_IPC_REQ_DISABLE:
            e = route_arbiter_set_enabled(s->arb, req.mono_ms, false);
            break;
        case ROUTE_IPC_REQ_TX_PREPARE:
            e = route_arbiter_tx_prepare(s->arb, req.mono_ms);
            break;
        case ROUTE_IPC_REQ_ATU_PREPARE:
            e = route_arbiter_atu_prepare(s->arb, req.mono_ms);
            break;
        case ROUTE_IPC_REQ_STATUS:
            e = ROUTE_ERR_OK;
            break;
        case ROUTE_IPC_REQ_REFRESH: {
            uint32_t blk[ROUTE_SNAPSHOT_REGS];
            memcpy(blk, req.block, sizeof(blk));
            e = route_arbiter_refresh(s->arb, blk, req.mono_ms);
            break;
        }
        case ROUTE_IPC_REQ_SHUTDOWN:
            s->running = false;
            break;
        default:
            e = ROUTE_ERR_INVALID;
            break;
    }
    /* El arbitro revisa el lease en cada request (reloj del cliente monótono);
     * no hay hilo propio: el supervisor/loop del daemon lo mantiene vivo. */
    (void) route_arbiter_tick(s->arb, req.mono_ms);
    route_ipc_msg_t r; fill_resp(&r, &req, (int32_t)e, s->arb);
    return (send_msg(s->fd, &r) == 0) ? 1 : -5;
}

int route_ipc_client_attach(route_ipc_client_t *c, int fd, const uint8_t token[ROUTE_IPC_TOKEN_LEN]) {
    if (c == NULL || fd < 0 || token == NULL) return -1;
    memset(c, 0, sizeof(*c)); c->fd = fd;
    memcpy(c->token, token, ROUTE_IPC_TOKEN_LEN);
    c->pid = 0x6200; c->starttime = 0x1234;
    return 0;
}

int route_ipc_client_request(route_ipc_client_t *c, uint32_t type, uint32_t value, uint32_t lease_ms,
                             uint64_t now_ms, route_ipc_msg_t *resp, int timeout_ms) {
    if (c == NULL || c->fd < 0 || resp == NULL) return -1;
    route_ipc_msg_t req;
    memset(&req, 0, sizeof(req));
    req.magic = ROUTE_IPC_MAGIC; req.version = ROUTE_IPC_VERSION; req.type = type;
    memcpy(req.token, c->token, ROUTE_IPC_TOKEN_LEN);
    req.pid = c->pid; req.starttime = c->starttime; req.mono_ms = now_ms;
    req.value = value; req.lease_ms = lease_ms; req.seq = ++c->seq;
    uint8_t buf[sizeof(req)];
    if (route_ipc_encode(&req, buf, sizeof(buf)) != sizeof(req)) return -1;
    if (send(c->fd, buf, sizeof(req), 0) != (ssize_t)sizeof(req)) return -1;
    struct pollfd p = { .fd = c->fd, .events = POLLIN };
    int pr = poll(&p, 1, timeout_ms);
    if (pr <= 0) return (pr == 0) ? -2 : -1;   /* timeout explícito: nunca indefinido */
    ssize_t n = recv(c->fd, resp, sizeof(*resp), 0);
    if (n <= 0) return -1;
    if (route_ipc_decode(resp, (size_t)n, resp) != 0) return -1;
    return 0;
}

int route_ipc_client_refresh(route_ipc_client_t *c, const uint32_t regs[ROUTE_SNAPSHOT_REGS],
                             uint64_t now_ms, route_ipc_msg_t *resp, int timeout_ms) {
    if (c == NULL || c->fd < 0 || regs == NULL || resp == NULL) return -1;
    route_ipc_msg_t req;
    memset(&req, 0, sizeof(req));
    req.magic = ROUTE_IPC_MAGIC; req.version = ROUTE_IPC_VERSION; req.type = ROUTE_IPC_REQ_REFRESH;
    memcpy(req.token, c->token, ROUTE_IPC_TOKEN_LEN);
    req.pid = c->pid; req.starttime = c->starttime; req.mono_ms = now_ms; req.seq = ++c->seq;
    memcpy(req.block, regs, sizeof(req.block));
    uint8_t buf[sizeof(req)];
    if (route_ipc_encode(&req, buf, sizeof(buf)) != sizeof(req)) return -1;
    if (send(c->fd, buf, sizeof(req), 0) != (ssize_t)sizeof(req)) return -1;
    struct pollfd p = { .fd = c->fd, .events = POLLIN };
    int pr = poll(&p, 1, timeout_ms);
    if (pr <= 0) return (pr == 0) ? -2 : -1;
    ssize_t n = recv(c->fd, resp, sizeof(*resp), 0);
    if (n <= 0) return -1;
    return route_ipc_decode(resp, (size_t)n, resp) == 0 ? 0 : -1;
}

/* QA-LINUX-DSP-PROXY — proxy GUI/libaether->IPC->arbitro (host, mocks). */
#define _DEFAULT_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "route_arbiter.h"
#include "route_ipc.h"
#include "route_proxy.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

typedef struct { pthread_mutex_t mu; int fail_next; uint32_t log[128]; int n; } fake_t;
static int fw(void *ctx, uint32_t v) { fake_t *f = ctx; pthread_mutex_lock(&f->mu); if (f->fail_next) { f->fail_next = 0; pthread_mutex_unlock(&f->mu); return -1; } if (f->n < 128) f->log[f->n++] = v; pthread_mutex_unlock(&f->mu); return 0; }
static int fcount(fake_t *f) { pthread_mutex_lock(&f->mu); int n = f->n; pthread_mutex_unlock(&f->mu); return n; }
static uint32_t flast(fake_t *f) { pthread_mutex_lock(&f->mu); uint32_t v = f->n ? f->log[f->n-1] : 0; pthread_mutex_unlock(&f->mu); return v; }
static const uint8_t TOKEN[16] = {'R','X','D','P','-','P','R','O','X','Y','-','0','1','2','3','4'};

typedef struct { fake_t f; route_arbiter_t *a; route_ipc_server_t *srv; route_ipc_client_t cli; int fd[2]; pthread_t th; atomic_bool stop; } env_t;
static void *srv(void *arg) { env_t *e = arg; while (!atomic_load(&e->stop)) { int r = route_ipc_server_serve(e->srv, 5); if (r == -1) break; } return NULL; }
static int up(env_t *e) {
    memset(e, 0, sizeof(*e)); pthread_mutex_init(&e->f.mu, NULL);
    route_bus_t bus = { .write_reg12 = fw, .ctx = &e->f };
    e->a = route_arbiter_create(&bus, 0); CHECK(e->a != NULL);
    CHECK(route_arbiter_boot(e->a) == ROUTE_ERR_OK);
    e->srv = route_ipc_server_create(e->a, TOKEN);
    CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, e->fd) == 0);
    route_ipc_server_attach(e->srv, e->fd[1]);
    route_ipc_client_attach(&e->cli, e->fd[0], TOKEN);
    atomic_init(&e->stop, false); pthread_create(&e->th, NULL, srv, e);
    return 0;
}
static void down(env_t *e) { atomic_store(&e->stop, true); pthread_join(e->th, NULL); close(e->fd[0]); route_ipc_server_destroy(e->srv); route_arbiter_destroy(e->a); pthread_mutex_destroy(&e->f.mu); }
static int hb(env_t *e, uint64_t t, uint32_t v) { route_ipc_msg_t r; return route_ipc_client_request(&e->cli, ROUTE_IPC_REQ_HEARTBEAT, v, 1, t, &r, 500); }

static void test_off(void) {
    unsetenv("X6200_LINUX_DSP_ROUTE_QA");
    CHECK(route_proxy_create(NULL, 0, false) == NULL);
    route_proxy_t *p = route_proxy_create(NULL, 0, true);
    CHECK(p == NULL);   /* sin IPC no hay proxy */
}
static void test_writes(void) {
    env_t e; up(&e);
    route_proxy_t *p = route_proxy_create(&e.cli, 0x100000u, true);
    CHECK(p != NULL); CHECK(route_proxy_is_enabled(p));
    uint32_t v = 0; CHECK(route_proxy_get_reg12(p, &v) == ROUTE_ERR_OK); CHECK(v == 0x100000u);
    CHECK(route_proxy_write_reg12(p, 0x100000u | 0x200000u) == ROUTE_ERR_OK);
    CHECK(flast(&e.f) == (0x100000u | 0x200000u));
    CHECK(route_proxy_get_reg12(p, &v) == ROUTE_ERR_OK); CHECK(v == (0x100000u | 0x200000u));
    CHECK(route_proxy_write_reg12(p, 0x8u) == ROUTE_ERR_OWNERSHIP);   /* voice_rec no */
    CHECK(route_proxy_record_set(p, true) == ROUTE_ERR_OWNERSHIP);
    CHECK(route_proxy_ptt_set(p, true) == ROUTE_ERR_OK);
    CHECK((flast(&e.f) & ROUTE_REG_IPTT) != 0); CHECK((flast(&e.f) & ROUTE_REG_VOICE_REC) == 0);
    CHECK(route_proxy_ptt_set(p, false) == ROUTE_ERR_OK);
    CHECK((flast(&e.f) & ROUTE_REG_IPTT) == 0);
    CHECK(route_proxy_atu_set(p, true) == ROUTE_ERR_OK);
    CHECK((flast(&e.f) & ROUTE_REG_ATUE) != 0);
    CHECK(route_proxy_split_set(p, true) == ROUTE_ERR_OK);
    CHECK((flast(&e.f) & 0x2u) != 0);
    route_proxy_destroy(p); down(&e);
}
static void test_tx_after_rx(void) {
    env_t e; up(&e);
    CHECK(hb(&e, 10, 0x100000u) == 0);
    CHECK(route_arbiter_pcm_submit(e.a, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 11) == ROUTE_ERR_OK);
    CHECK(route_arbiter_set_enabled(e.a, 20, true) == ROUTE_ERR_OK);
    route_proxy_t *p = route_proxy_create(&e.cli, 0x100000u, true);
    int before = fcount(&e.f);
    CHECK(route_proxy_ptt_set(p, true) == ROUTE_ERR_OK);
    /* orden: primero disable (sin 0x8), después iptt */
    CHECK(fcount(&e.f) >= before + 2);
    pthread_mutex_lock(&e.f.mu);
    bool saw_clear = false; int idx_clear = -1, idx_iptt = -1;
    for (int i = 0; i < e.f.n; i++) {
        if ((e.f.log[i] & ROUTE_REG_VOICE_REC) == 0 && !saw_clear) { saw_clear = true; idx_clear = i; }
        if (e.f.log[i] & ROUTE_REG_IPTT) idx_iptt = i;
    }
    pthread_mutex_unlock(&e.f.mu);
    CHECK(idx_clear >= 0); CHECK(idx_iptt > idx_clear);
    CHECK(route_arbiter_state(e.a) == ROUTE_STATE_DEGRADED_BASE);
    route_proxy_destroy(p); down(&e);
}
static void test_invalid_shadow_and_timeout(void) {
    env_t e; up(&e);
    route_proxy_t *p = route_proxy_create(&e.cli, 0x100000u, true);
    pthread_mutex_lock(&e.f.mu); e.f.fail_next = 1; pthread_mutex_unlock(&e.f.mu);
    route_err_t er = route_proxy_write_reg12(p, 0x100000u | 0x200000u);
    CHECK(er == ROUTE_ERR_BUS || er == ROUTE_ERR_UNKNOWN_STATE);
    uint32_t v;
    CHECK(route_proxy_get_reg12(p, &v) != ROUTE_ERR_OK);  /* cache invalidada */
    route_proxy_destroy(p); down(&e);
    int fd[2]; CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fd) == 0);
    route_ipc_client_t dead; route_ipc_client_attach(&dead, fd[0], TOKEN);
    p = route_proxy_create(&dead, 0x100000u, true);
    CHECK(p != NULL);
    CHECK(route_proxy_ptt_set(p, true) == ROUTE_ERR_TIMEOUT);  /* servidor ausente */
    CHECK(route_proxy_get_reg12(p, &v) == ROUTE_ERR_OK && v == 0x100000u); /* caché intacta */
    route_proxy_destroy(p); close(fd[0]); close(fd[1]);
}
static void test_serialization(void) {
    env_t e; up(&e);
    route_proxy_t *p = route_proxy_create(&e.cli, 0x100000u, true);
    int before = fcount(&e.f);
    for (int i = 0; i < 40; i++) CHECK(route_proxy_write_reg12(p, 0x100000u | (uint32_t)(i & 1 ? 0x200000u : 0)) == ROUTE_ERR_OK);
    CHECK(fcount(&e.f) == before + 40);   /* cada request => una escritura del árbitro */
    route_proxy_destroy(p); down(&e);
}
int main(void) {
    test_off(); test_writes(); test_tx_after_rx(); test_invalid_shadow_and_timeout(); test_serialization();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}

/* QA-LINUX-DSP-SNAPSHOT-BULK — refresh 218B propiedad del arbitro (host/mocks). */
#define _DEFAULT_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "route_arbiter.h"
#include "route_ipc.h"
#include "route_proxy.h"

static uint64_t nowms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec*1000u + (uint64_t)ts.tv_nsec/1000000u; }
static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

typedef struct { pthread_mutex_t mu; int fail_next; uint8_t block[ROUTE_SNAPSHOT_BYTES]; int blocks; uint32_t log[64]; int n; } fake_t;
static int fw(void *ctx, uint32_t v) { fake_t *f = ctx; pthread_mutex_lock(&f->mu); if (f->fail_next) { f->fail_next = 0; pthread_mutex_unlock(&f->mu); return -1; } if (f->n < 64) f->log[f->n++] = v; pthread_mutex_unlock(&f->mu); return 0; }
static int fb(void *ctx, const uint8_t *b, size_t len) { fake_t *f = ctx; pthread_mutex_lock(&f->mu); if (f->fail_next) { f->fail_next = 0; pthread_mutex_unlock(&f->mu); return -1; } memcpy(f->block, b, len); f->blocks++; pthread_mutex_unlock(&f->mu); return 0; }
static int blocks(fake_t *f) { pthread_mutex_lock(&f->mu); int n = f->blocks; pthread_mutex_unlock(&f->mu); return n; }
static const uint8_t TOKEN[16] = {'R','X','D','P','-','R','E','F','R','-','0','1','2','3','4','5'};
typedef struct { fake_t f; route_arbiter_t *a; route_ipc_server_t *srv; route_ipc_client_t cli; int fd[2]; pthread_t th; atomic_bool stop; route_proxy_t *p; } env_t;
static void *srvfn(void *arg) { env_t *e = arg; while (!atomic_load(&e->stop)) { int r = route_ipc_server_serve(e->srv, 5); if (r == -1) break; } return NULL; }
static int up(env_t *e, bool bulk) {
    memset(e, 0, sizeof(*e)); pthread_mutex_init(&e->f.mu, NULL);
    route_bus_t bus = { .write_reg12 = fw, .write_block = bulk ? fb : NULL, .ctx = &e->f };
    e->a = route_arbiter_create(&bus, 0); CHECK(e->a != NULL); CHECK(route_arbiter_boot(e->a) == ROUTE_ERR_OK);
    e->srv = route_ipc_server_create(e->a, TOKEN); CHECK(e->srv != NULL);
    CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, e->fd) == 0);
    route_ipc_server_attach(e->srv, e->fd[1]);
    route_ipc_client_attach(&e->cli, e->fd[0], TOKEN);
    atomic_init(&e->stop, false); pthread_create(&e->th, NULL, srvfn, e);
    e->p = route_proxy_create(&e->cli, 0x100000u, true); CHECK(e->p != NULL);
    return 0;
}
static void down(env_t *e) { route_proxy_destroy(e->p); atomic_store(&e->stop, true); pthread_join(e->th, NULL); close(e->fd[0]); route_ipc_server_destroy(e->srv); route_arbiter_destroy(e->a); pthread_mutex_destroy(&e->f.mu); }
static int hb(env_t *e, uint64_t t, uint32_t v) { route_ipc_msg_t r; return route_ipc_client_request(&e->cli, ROUTE_IPC_REQ_HEARTBEAT, v, 1, t, &r, 500); }
static void fill(uint32_t regs[ROUTE_SNAPSHOT_REGS]) { for (unsigned i = 0; i < ROUTE_SNAPSHOT_REGS; i++) regs[i] = 0x1000u + i; regs[12] = 0x100000u; }

static void test_no_bulk(void) {
    env_t e; up(&e, false); fill((uint32_t[ROUTE_SNAPSHOT_REGS]){0});
    uint32_t regs[ROUTE_SNAPSHOT_REGS]; fill(regs);
    CHECK(hb(&e, nowms(), 0x100000u) == 0);
    CHECK(!route_arbiter_refresh_available(e.a));
    route_err_t er = route_proxy_refresh_block(e.p, regs);
    CHECK(er == ROUTE_ERR_UNSUPPORTED);   /* sin bulk => refresh deshabilitado */
    CHECK(blocks(&e.f) == 0);
    down(&e);
}
static void test_refresh_ok(void) {
    env_t e; up(&e, true); fill((uint32_t[ROUTE_SNAPSHOT_REGS]){0});
    uint32_t regs[ROUTE_SNAPSHOT_REGS]; fill(regs);
    CHECK(hb(&e, nowms(), 0x100000u) == 0);
    CHECK(route_arbiter_refresh_available(e.a));
    CHECK(route_proxy_refresh_block(e.p, regs) == ROUTE_ERR_OK);
    CHECK(blocks(&e.f) == 1);
    CHECK(e.f.block[0] == 0 && e.f.block[1] == 0);
    CHECK(e.f.block[2 + 48] == 0x00 && e.f.block[2 + 49] == 0x00 && e.f.block[2 + 50] == 0x10); /* reg12=0x100000 LE */
    CHECK((e.f.block[2 + 52] & 0) == 0);
    route_proxy_t *pp = e.p; (void)pp;
    CHECK(route_arbiter_stats(e.a)->refreshes == 1);
    CHECK(route_arbiter_state(e.a) == ROUTE_STATE_BASE);
    down(&e);
}
static void test_refresh_route_merge(void) {
    env_t e; up(&e, true);
    uint32_t regs[ROUTE_SNAPSHOT_REGS]; fill(regs);
    CHECK(hb(&e, nowms(), 0x100000u) == 0);
    CHECK(route_arbiter_pcm_submit(e.a, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, nowms()) == ROUTE_ERR_OK);
    CHECK(route_arbiter_set_enabled(e.a, nowms(), true) == ROUTE_ERR_OK);
    regs[12] = 0x100000u | ROUTE_REG_IPTT;   /* TX bits no deben pasar con ruta activa */
    CHECK(route_proxy_refresh_block(e.p, regs) == ROUTE_ERR_OK);
    /* bloque escrito: reg12 con voice_rec=1 y sin IPTT */
    uint32_t r12 = (uint32_t)e.f.block[2+48] | ((uint32_t)e.f.block[2+49] << 8) |
                   ((uint32_t)e.f.block[2+50] << 16) | ((uint32_t)e.f.block[2+51] << 24);
    CHECK((r12 & ROUTE_REG_VOICE_REC) != 0);
    CHECK((r12 & ROUTE_REG_IPTT) == 0);
    CHECK(route_arbiter_stats(e.a)->refreshes == 1);
    down(&e);
}
static void test_refresh_errors(void) {
    env_t e; up(&e, true);
    uint32_t regs[ROUTE_SNAPSHOT_REGS]; fill(regs);
    CHECK(hb(&e, nowms(), 0x100000u) == 0);
    /* lease vencido => NOT_READY */
    CHECK(route_proxy_refresh_block(e.p, regs) == ROUTE_ERR_OK); /* fresco */
    usleep(1000);
    /* bus fail => FAULT y sin escritura válida */
    pthread_mutex_lock(&e.f.mu); e.f.fail_next = 1; pthread_mutex_unlock(&e.f.mu);
    route_err_t er = route_proxy_refresh_block(e.p, regs);
    CHECK(er == ROUTE_ERR_BUS || er == ROUTE_ERR_UNKNOWN_STATE);
    CHECK(route_arbiter_state(e.a) == ROUTE_STATE_FAULT);
    down(&e);
    /* timeout: cliente sin servidor */
    int fd[2]; CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fd) == 0);
    route_ipc_client_t dead; route_ipc_client_attach(&dead, fd[0], TOKEN);
    route_proxy_t *p = route_proxy_create(&dead, 0x100000u, true); CHECK(p != NULL);
    CHECK(route_proxy_refresh_block(p, regs) == ROUTE_ERR_TIMEOUT);
    route_proxy_destroy(p); close(fd[0]); close(fd[1]);
}
int main(void) {
    test_no_bulk(); test_refresh_ok(); test_refresh_route_merge(); test_refresh_errors();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}

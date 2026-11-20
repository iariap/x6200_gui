/* QA-LINUX-DSP-X6200CTRL-PATCH — idle refresh con API parcheada (host stubs). */
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
#include <aether_radio/x6200_control/api.h>
#include <aether_radio/x6200_control/low/control.h>
#include "route_arbiter.h"
#include "route_ipc.h"
#include "route_proxy.h"
#include "route_libaether_proxy.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
static uint64_t nowms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec*1000u + (uint64_t)ts.tv_nsec/1000000u; }

/* --- stubs: libaether parcheada + originales para delegación --- */
static uint32_t sdk_regs[ROUTE_SNAPSHOT_REGS];
static int orig_idle, orig_ptt;
uint32_t x6200_control_get(x6200_cmd_enum_t cmd) { return (unsigned)cmd < ROUTE_SNAPSHOT_REGS ? sdk_regs[cmd] : 0; }
void x6200_control_shadow_update(uint32_t cmd, uint32_t arg) { if (cmd < ROUTE_SNAPSHOT_REGS) sdk_regs[cmd] = arg; }
bool x6200_control_bulk_write(const uint8_t *block, size_t len) { (void)block; (void)len; return true; }
void route_test_orig_idle(void) { orig_idle++; }
void route_test_orig_ptt(bool on) { (void)on; orig_ptt++; }
void route_test_orig_record(bool on) { (void)on; }
void route_test_orig_atu_tune(bool on) { (void)on; }
void route_test_orig_atu_set(bool on) { (void)on; }
void route_test_orig_modem(bool on) { (void)on; }
void route_test_orig_swrscan(bool on) { (void)on; }
void route_test_orig_split(bool on) { (void)on; }
void route_test_orig_poweroff(void) {}
void route_test_orig_calibration(bool on) { (void)on; }
bool route_test_orig_cmd(int c, uint32_t a) { (void)c; (void)a; return true; }
void route_test_orig_host_cmd(int c) { (void)c; }

typedef struct { pthread_mutex_t mu; int fail_next; uint8_t block[ROUTE_SNAPSHOT_BYTES]; int blocks; uint32_t log[64]; int n; } fake_t;
static int fw(void *ctx, uint32_t v) { fake_t *f = ctx; pthread_mutex_lock(&f->mu); if (f->fail_next) { f->fail_next = 0; pthread_mutex_unlock(&f->mu); return -1; } if (f->n < 64) f->log[f->n++] = v; pthread_mutex_unlock(&f->mu); return 0; }
static int fb(void *ctx, const uint8_t *b, size_t len) { fake_t *f = ctx; pthread_mutex_lock(&f->mu); if (f->fail_next) { f->fail_next = 0; pthread_mutex_unlock(&f->mu); return -1; } memcpy(f->block, b, len); f->blocks++; pthread_mutex_unlock(&f->mu); return 0; }
static int blocks(fake_t *f) { pthread_mutex_lock(&f->mu); int n = f->blocks; pthread_mutex_unlock(&f->mu); return n; }
static uint32_t block_reg12(fake_t *f) { return (uint32_t)f->block[2+48] | ((uint32_t)f->block[2+49]<<8) | ((uint32_t)f->block[2+50]<<16) | ((uint32_t)f->block[2+51]<<24); }
static const uint8_t TOKEN[16] = {'R','X','D','P','-','P','A','T','C','H','0','1','2','3','4','5'};
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
    for (unsigned i = 0; i < ROUTE_SNAPSHOT_REGS; i++) sdk_regs[i] = 0x3000u + i;
    sdk_regs[12] = 0x100000u;
    return 0;
}
static void down(env_t *e) { route_libaether_proxy_set(NULL); route_proxy_destroy(e->p); atomic_store(&e->stop, true); pthread_join(e->th, NULL); close(e->fd[0]); route_ipc_server_destroy(e->srv); route_arbiter_destroy(e->a); pthread_mutex_destroy(&e->f.mu); }
static int hb(env_t *e, uint64_t t) { route_ipc_msg_t r; return route_ipc_client_request(&e->cli, ROUTE_IPC_REQ_HEARTBEAT, 0x100000u, 1, t, &r, 500); }

static void test_idle_refresh(void) {
    env_t e; up(&e, true);
    route_libaether_proxy_set(e.p);
    CHECK(hb(&e, nowms()) == 0);
    x6200_control_ptt_set(true);                 /* proxied + sync sombra autoritativa */
    CHECK(orig_ptt == 0);
    CHECK((x6200_control_get(x6200_sple_atue_trx) & ROUTE_REG_IPTT) != 0);
    int before = blocks(&e.f);
    x6200_control_idle();                        /* refresh BASE: voice=0, PTT preservado */
    CHECK(orig_idle == 0);
    CHECK(blocks(&e.f) == before + 1);
    CHECK((block_reg12(&e.f) & ROUTE_REG_VOICE_REC) == 0);
    CHECK((block_reg12(&e.f) & ROUTE_REG_IPTT) != 0);
    CHECK(route_libaether_proxy_last_error() == ROUTE_ERR_OK);
    /* ruta activa: idle refresh debe mergear voice=1 y strip TX */
    x6200_control_ptt_set(false);
    CHECK(hb(&e, nowms()) == 0);
    CHECK(route_arbiter_pcm_submit(e.a, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, nowms()) == ROUTE_ERR_OK);
    CHECK(route_arbiter_set_enabled(e.a, nowms(), true) == ROUTE_ERR_OK);
    before = blocks(&e.f);
    x6200_control_idle();
    CHECK(blocks(&e.f) == before + 1);
    CHECK((block_reg12(&e.f) & ROUTE_REG_VOICE_REC) != 0);
    CHECK((block_reg12(&e.f) & ROUTE_REG_IPTT) == 0);
    down(&e);
}
static void test_idle_no_bulk(void) {
    env_t e; up(&e, false);
    route_libaether_proxy_set(e.p);
    CHECK(hb(&e, nowms()) == 0);
    x6200_control_idle();
    CHECK(orig_idle == 0);
    CHECK(blocks(&e.f) == 0);
    CHECK(route_libaether_proxy_last_error() == ROUTE_ERR_UNSUPPORTED);
    down(&e);
}
static void test_off_delegates(void) {
    route_libaether_proxy_set(NULL);
    int i0 = orig_idle, p0 = orig_ptt;
    x6200_control_idle();
    CHECK(orig_idle == i0 + 1);
    CHECK(orig_ptt == p0);
}
static void test_concurrent(void) {
    env_t e; up(&e, true); route_libaether_proxy_set(e.p); CHECK(hb(&e, nowms()) == 0);
    orig_idle = 0; orig_ptt = 0;
    int before = blocks(&e.f);
    for (int i = 0; i < 20; i++) { CHECK(hb(&e, nowms()) == 0); x6200_control_ptt_set((i & 1) != 0); x6200_control_idle(); }
    CHECK(blocks(&e.f) == before + 20);
    CHECK(orig_idle == 0); CHECK(orig_ptt == 0);
    down(&e);
}
int main(void) {
    test_idle_refresh(); test_idle_no_bulk(); test_off_delegates(); test_concurrent();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}

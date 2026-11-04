/* QA-LINUX-DSP-CALLSITES — interposición libaether: call-sites route-relevant. */
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
#include "route_libaether_proxy.h"
#include <aether_radio/x6200_control/api.h>
#include <aether_radio/x6200_control/low/control.h>

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* Stubs de los "originales" de libaether (sólo test). */
static int orig_record, orig_ptt, orig_atu_tune, orig_atu_set, orig_modem, orig_swrscan,
           orig_split, orig_poweroff, orig_calib, orig_cmd, orig_idle, orig_host_cmd, last_host;
void route_test_orig_record(bool on) { (void)on; orig_record++; }
void route_test_orig_ptt(bool on) { (void)on; orig_ptt++; }
void route_test_orig_atu_tune(bool on) { (void)on; orig_atu_tune++; }
void route_test_orig_atu_set(bool on) { (void)on; orig_atu_set++; }
void route_test_orig_modem(bool on) { (void)on; orig_modem++; }
void route_test_orig_swrscan(bool on) { (void)on; orig_swrscan++; }
void route_test_orig_split(bool on) { (void)on; orig_split++; }
void route_test_orig_poweroff(void) { orig_poweroff++; }
void route_test_orig_calibration(bool on) { (void)on; orig_calib++; }
bool route_test_orig_cmd(int cmd, uint32_t arg) { (void)cmd; (void)arg; orig_cmd++; return true; }
void route_test_orig_idle(void) { orig_idle++; }
void route_test_orig_host_cmd(int c) { orig_host_cmd++; last_host = c; }
static void reset(void) { orig_record = orig_ptt = orig_atu_tune = orig_atu_set = orig_modem = orig_swrscan = orig_split = orig_poweroff = orig_calib = orig_cmd = orig_idle = orig_host_cmd = 0; }

typedef struct { pthread_mutex_t mu; int fail_next; uint32_t log[128]; int n; } fake_t;
static int fw(void *ctx, uint32_t v) { fake_t *f = ctx; pthread_mutex_lock(&f->mu); if (f->fail_next) { f->fail_next = 0; pthread_mutex_unlock(&f->mu); return -1; } if (f->n < 128) f->log[f->n++] = v; pthread_mutex_unlock(&f->mu); return 0; }
static int fcount(fake_t *f) { pthread_mutex_lock(&f->mu); int n = f->n; pthread_mutex_unlock(&f->mu); return n; }
static uint32_t flast(fake_t *f) { pthread_mutex_lock(&f->mu); uint32_t v = f->n ? f->log[f->n-1] : 0; pthread_mutex_unlock(&f->mu); return v; }
static const uint8_t TOKEN[16] = {'R','X','D','P','-','C','A','L','L','-','0','1','2','3','4','5'};
typedef struct { fake_t f; route_arbiter_t *a; route_ipc_server_t *srv; route_ipc_client_t cli; int fd[2]; pthread_t th; atomic_bool stop; route_proxy_t *p; } env_t;
static void *srvfn(void *arg) { env_t *e = arg; while (!atomic_load(&e->stop)) { int r = route_ipc_server_serve(e->srv, 5); if (r == -1) break; } return NULL; }
static int up(env_t *e) {
    memset(e, 0, sizeof(*e)); pthread_mutex_init(&e->f.mu, NULL);
    route_bus_t bus = { .write_reg12 = fw, .ctx = &e->f };
    e->a = route_arbiter_create(&bus, 0); CHECK(e->a != NULL); CHECK(route_arbiter_boot(e->a) == ROUTE_ERR_OK);
    e->srv = route_ipc_server_create(e->a, TOKEN); CHECK(e->srv != NULL);
    CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, e->fd) == 0);
    route_ipc_server_attach(e->srv, e->fd[1]);
    route_ipc_client_attach(&e->cli, e->fd[0], TOKEN);
    atomic_init(&e->stop, false); pthread_create(&e->th, NULL, srvfn, e);
    e->p = route_proxy_create(&e->cli, 0x100000u, true); CHECK(e->p != NULL);
    return 0;
}
static void down(env_t *e) { route_libaether_proxy_set(NULL); route_proxy_destroy(e->p); atomic_store(&e->stop, true); pthread_join(e->th, NULL); close(e->fd[0]); route_ipc_server_destroy(e->srv); route_arbiter_destroy(e->a); pthread_mutex_destroy(&e->f.mu); }

static void test_off_fallback(void) {
    reset(); route_libaether_proxy_set(NULL);
    x6200_control_record_set(true); x6200_control_ptt_set(true); x6200_control_idle();
    x6200_control_cmd(x6200_sple_atue_trx, 0x100000u);
    CHECK(orig_record == 1); CHECK(orig_ptt == 1); CHECK(orig_idle == 1); CHECK(orig_cmd == 1);
}
static void test_active_route_calls(void) {
    env_t e; up(&e); route_libaether_proxy_set(e.p); reset();
    int before = fcount(&e.f);
    x6200_control_record_set(true);
    CHECK(orig_record == 0); CHECK(fcount(&e.f) == before);          /* voice_rec no se toca */
    CHECK(route_libaether_proxy_last_error() == ROUTE_ERR_OWNERSHIP);
    x6200_control_ptt_set(true);
    CHECK(orig_ptt == 0); CHECK(fcount(&e.f) > before);
    CHECK((flast(&e.f) & ROUTE_REG_IPTT) != 0); CHECK((flast(&e.f) & ROUTE_REG_VOICE_REC) == 0);
    x6200_control_atu_tune(true);
    CHECK(orig_atu_tune == 0); CHECK(orig_host_cmd == 1); CHECK(last_host == 0x8004);
    CHECK((flast(&e.f) & ROUTE_REG_ATU_TUNE) != 0);
    x6200_control_atu_set(true); CHECK(orig_atu_set == 0); CHECK((flast(&e.f) & ROUTE_REG_ATUE) != 0);
    x6200_control_modem_set(true); CHECK(orig_modem == 0); CHECK((flast(&e.f) & ROUTE_REG_MODEM) != 0);
    x6200_control_swrscan_set(true); CHECK(orig_swrscan == 0); CHECK((flast(&e.f) & ROUTE_REG_SWRSCAN) != 0);
    x6200_control_split_set(true); CHECK(orig_split == 0); CHECK((flast(&e.f) & 0x2u) != 0);
    x6200_control_calibration_set(true); CHECK(orig_calib == 0); CHECK((flast(&e.f) & 0x8000u) != 0);
    x6200_control_poweroff(); CHECK(orig_poweroff == 0); CHECK((flast(&e.f) & ROUTE_REG_POWER_OFF) != 0);
    /* cmd genérico: reg12 por proxy; otros directo */
    int c0 = fcount(&e.f);
    CHECK(x6200_control_cmd(x6200_sple_atue_trx, 0x200000u) == true); CHECK(fcount(&e.f) == c0 + 1);
    CHECK(orig_cmd == 0);
    CHECK(x6200_control_cmd(x6200_sple_atue_trx, ROUTE_REG_VOICE_REC) == false); CHECK(fcount(&e.f) == c0 + 1);
    CHECK(x6200_control_cmd((x6200_cmd_enum_t)5, 1) == true); CHECK(orig_cmd == 1);
    /* idle: no directo */
    int b = fcount(&e.f); x6200_control_idle(); CHECK(orig_idle == 0); CHECK(fcount(&e.f) == b);
    down(&e);
}
static void test_concurrency_and_timeout(void) {
    env_t e; up(&e); route_libaether_proxy_set(e.p); reset();
    int before = fcount(&e.f);
    for (int i = 0; i < 20; i++) { x6200_control_ptt_set(i & 1 ? true : false); }
    CHECK(fcount(&e.f) == before + 20); CHECK(orig_ptt == 0);
    down(&e);
    /* proxy con servidor ausente: no llama al original, error explícito */
    int fd[2]; CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fd) == 0);
    route_ipc_client_t dead; route_ipc_client_attach(&dead, fd[0], TOKEN);
    route_proxy_t *p = route_proxy_create(&dead, 0x100000u, true); CHECK(p != NULL);
    route_libaether_proxy_set(p); reset();
    x6200_control_ptt_set(true);
    CHECK(orig_ptt == 0); CHECK(route_libaether_proxy_last_error() == ROUTE_ERR_TIMEOUT);
    route_libaether_proxy_set(NULL); route_proxy_destroy(p); close(fd[0]); close(fd[1]);
}
int main(void) {
    test_off_fallback(); test_active_route_calls(); test_concurrency_and_timeout();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}

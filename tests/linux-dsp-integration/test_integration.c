/* QA-LINUX-DSP-INTEGRATION — tests host de la infraestructura integrada (IPC+arbitro+supervisor).
 * Sin hardware: bus mock. DEFAULT OFF; no activa nada real. */
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "route_arbiter.h"
#include "route_ipc.h"
#include "route_supervisor.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

typedef struct {
    pthread_mutex_t mu;
    int fail_next;
    uint32_t log[256];
    int n;
} fake_t;
static int fake_write(void *ctx, uint32_t value) {
    fake_t *f = ctx;
    pthread_mutex_lock(&f->mu);
    if (f->fail_next) { f->fail_next = 0; pthread_mutex_unlock(&f->mu); return -1; }
    if (f->n < 256) f->log[f->n++] = value;
    pthread_mutex_unlock(&f->mu);
    return 0;
}
static int bus_count(fake_t *f) { pthread_mutex_lock(&f->mu); int n = f->n; pthread_mutex_unlock(&f->mu); return n; }
static uint32_t bus_last(fake_t *f) { pthread_mutex_lock(&f->mu); uint32_t v = f->n ? f->log[f->n-1] : 0; pthread_mutex_unlock(&f->mu); return v; }

static const uint8_t TOKEN[16] = {'R','X','D','P','-','T','O','K','E','N','-','0','1','2','3','4'};

typedef struct {
    fake_t fake;
    route_arbiter_t *arb;
    route_ipc_server_t *srv;
    route_ipc_client_t cli;
    int fd[2];
    pthread_t th;
    atomic_bool stop;
    int drive_now;
} env_t;

static void *server_thread(void *arg) {
    env_t *e = arg;
    while (!atomic_load(&e->stop)) {
        int r = route_ipc_server_serve(e->srv, 5);
        if (r == -1) break;
    }
    return NULL;
}
static int env_setup(env_t *e, bool boot) {
    memset(e, 0, sizeof(*e));
    pthread_mutex_init(&e->fake.mu, NULL);
    route_bus_t bus = { .write_reg12 = fake_write, .ctx = &e->fake };
    e->arb = route_arbiter_create(&bus, 0);
    if (!e->arb) return -1;
    e->srv = route_ipc_server_create(e->arb, TOKEN);
    if (!e->srv) return -1;
    if (boot && route_arbiter_boot(e->arb) != ROUTE_ERR_OK) return -1;
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, e->fd) != 0) return -1;
    route_ipc_server_attach(e->srv, e->fd[1]);
    route_ipc_client_attach(&e->cli, e->fd[0], TOKEN);
    atomic_init(&e->stop, false);
    if (pthread_create(&e->th, NULL, server_thread, e) != 0) return -1;
    return 0;
}
static void env_teardown(env_t *e) {
    atomic_store(&e->stop, true);
    pthread_join(e->th, NULL);
    close(e->fd[0]);
    route_ipc_server_destroy(e->srv);
    route_arbiter_destroy(e->arb);
    pthread_mutex_destroy(&e->fake.mu);
}
static int req(env_t *e, uint32_t type, uint32_t value, uint32_t lease, uint64_t now, route_ipc_msg_t *r) {
    return route_ipc_client_request(&e->cli, type, value, lease, now, r, 500);
}
static int hb(env_t *e, uint64_t now, uint32_t value, bool ok) { route_ipc_msg_t r; return req(e, ROUTE_IPC_REQ_HEARTBEAT, value, ok ? 1 : 0, now, &r); }
static int enable(env_t *e, uint64_t now) { route_ipc_msg_t r; int rc = req(e, ROUTE_IPC_REQ_ENABLE, 0, 0, now, &r); return rc == 0 ? r.err : -100; }

/* 1. boot/base: sin auto-enable; shadow válido */
static void test_boot_base(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    route_ipc_msg_t r; CHECK(req(&e, ROUTE_IPC_REQ_STATUS, 0, 0, 10, &r) == 0);
    CHECK(r.err == ROUTE_ERR_OK); CHECK(r.state == ROUTE_STATE_BASE);
    CHECK(enable(&e, 11) == ROUTE_ERR_NOT_READY);
    CHECK(bus_count(&e.fake) == 1); CHECK(bus_last(&e.fake) == 0);
    env_teardown(&e);
}
/* 2. boot falla => FAULT y no habilita */
static void test_boot_fail(void) {
    env_t e; memset(&e, 0, sizeof(e)); pthread_mutex_init(&e.fake.mu, NULL); e.fake.fail_next = 1;
    route_bus_t bus = { .write_reg12 = fake_write, .ctx = &e.fake };
    e.arb = route_arbiter_create(&bus, 0); e.srv = route_ipc_server_create(e.arb, TOKEN);
    CHECK(route_arbiter_boot(e.arb) == ROUTE_ERR_BUS);
    CHECK(route_arbiter_state(e.arb) == ROUTE_STATE_FAULT);
    CHECK(route_arbiter_set_enabled(e.arb, 10, true) != ROUTE_ERR_OK);
    route_ipc_server_destroy(e.srv); route_arbiter_destroy(e.arb); pthread_mutex_destroy(&e.fake.mu);
}
/* 3. heartbeat + PCM + enable => RX_LINUX sólo bit 0x8 */
static void test_enable(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(hb(&e, 20, 0x100000u, true) == 0);
    CHECK(route_arbiter_pcm_submit(e.arb, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 21) == ROUTE_ERR_OK);
    CHECK(enable(&e, 30) == ROUTE_ERR_OK);
    route_ipc_msg_t r; CHECK(req(&e, ROUTE_IPC_REQ_STATUS, 0, 0, 31, &r) == 0);
    CHECK(r.state == ROUTE_STATE_RX_LINUX);
    CHECK(bus_last(&e.fake) == 0x100008u);          /* sólo cambió 0x8 */
    env_teardown(&e);
}
/* 4. heartbeat write_ok=false bloquea enable */
static void test_heartbeat_write_fail(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(hb(&e, 20, 0, false) == 0);
    CHECK(route_arbiter_pcm_submit(e.arb, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 21) == ROUTE_ERR_OK);
    CHECK(enable(&e, 30) == ROUTE_ERR_NOT_READY);
    CHECK(bus_count(&e.fake) == 1);
    env_teardown(&e);
}
/* 5. TX con RX activo: disable-first y luego PTT; bits preservados */
static void test_tx_ordering(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(hb(&e, 20, 0x100000u, true) == 0);
    CHECK(route_arbiter_pcm_submit(e.arb, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 21) == ROUTE_ERR_OK);
    CHECK(enable(&e, 30) == ROUTE_ERR_OK);
    route_ipc_msg_t r; CHECK(req(&e, ROUTE_IPC_REQ_TX_PREPARE, 0, 0, 40, &r) == 0); CHECK(r.err == ROUTE_ERR_OK);
    CHECK(r.state == ROUTE_STATE_DEGRADED_BASE);
    uint32_t after_disable = bus_last(&e.fake); CHECK((after_disable & ROUTE_REG_VOICE_REC) == 0);
    CHECK((after_disable & ~ROUTE_REG_VOICE_REC) == 0x100000u);
    CHECK(req(&e, ROUTE_IPC_REQ_APPLY_SHARED, ROUTE_REG_IPTT | 0x100000u, 0, 41, &r) == 0);
    CHECK(r.err == ROUTE_ERR_OK);
    CHECK(bus_last(&e.fake) == (ROUTE_REG_IPTT | 0x100000u));
    env_teardown(&e);
}
/* 6. bus falla en tx_prepare => FAULT */
static void test_tx_bus_fail(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(hb(&e, 20, 0, true) == 0);
    CHECK(route_arbiter_pcm_submit(e.arb, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 21) == ROUTE_ERR_OK);
    CHECK(enable(&e, 30) == ROUTE_ERR_OK);
    pthread_mutex_lock(&e.fake.mu); e.fake.fail_next = 1; pthread_mutex_unlock(&e.fake.mu);
    route_ipc_msg_t r; CHECK(req(&e, ROUTE_IPC_REQ_TX_PREPARE, 0, 0, 40, &r) == 0);
    CHECK(r.err == ROUTE_ERR_BUS); CHECK(r.state == ROUTE_STATE_FAULT);
    CHECK(enable(&e, 50) == ROUTE_ERR_UNKNOWN_STATE);
    env_teardown(&e);
}
/* 7. token inválido y PID ajeno rechazados; secuencia duplicada => STALE */
static route_ipc_msg_t raw_msg(uint32_t type, uint32_t seq, uint32_t value, uint32_t pid, uint32_t start) {
    route_ipc_msg_t m; memset(&m, 0, sizeof(m));
    m.magic = ROUTE_IPC_MAGIC; m.version = ROUTE_IPC_VERSION; m.type = type;
    memcpy(m.token, TOKEN, 16); m.pid = pid; m.starttime = start; m.mono_ms = 100; m.seq = seq; m.value = value; m.lease_ms = 1;
    return m;
}
static int raw_req(env_t *e, route_ipc_msg_t m, route_ipc_msg_t *r) {
    if (send(e->fd[0], &m, sizeof(m), 0) != (ssize_t)sizeof(m)) return -1;
    return recv(e->fd[0], r, sizeof(*r), 0) == (ssize_t)sizeof(*r) ? 0 : -2;
}
static void test_ownership_and_stale(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    route_ipc_msg_t r;
    route_ipc_msg_t bad = raw_msg(ROUTE_IPC_REQ_HEARTBEAT, 1, 0, 1, 1);
    memset(bad.token, 0, 16); CHECK(raw_req(&e, bad, &r) == 0); CHECK(r.err == ROUTE_ERR_OWNERSHIP);
    CHECK(hb(&e, 210, 0, true) == 0);                 /* cliente legítimo (pid del cliente) */
    route_ipc_msg_t foreign = raw_msg(ROUTE_IPC_REQ_STATUS, 3, 0, 0xBEEF, 1);
    CHECK(raw_req(&e, foreign, &r) == 0); CHECK(r.err == ROUTE_ERR_OWNERSHIP);
    env_teardown(&e);
}
/* 8. apply_shared no puede tocar voice_rec */
static void test_shared_voice_rejected(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(hb(&e, 20, 0, true) == 0);
    route_ipc_msg_t r; CHECK(req(&e, ROUTE_IPC_REQ_APPLY_SHARED, ROUTE_REG_VOICE_REC, 0, 21, &r) == 0);
    CHECK(r.err == ROUTE_ERR_OWNERSHIP);
    env_teardown(&e);
}
/* 9. lease vencido => restore sin GUI y <=250 ms */
static void test_lease_restore(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(hb(&e, 20, 0, true) == 0);
    CHECK(route_arbiter_pcm_submit(e.arb, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 21) == ROUTE_ERR_OK);
    CHECK(enable(&e, 30) == ROUTE_ERR_OK);
    route_ipc_msg_t r; CHECK(req(&e, ROUTE_IPC_REQ_STATUS, 0, 0, 160, &r) == 0);
    CHECK(r.state == ROUTE_STATE_DEGRADED_BASE);
    CHECK(bus_last(&e.fake) & ROUTE_REG_VOICE_REC ? false : true);
    CHECK((bus_last(&e.fake) & ~ROUTE_REG_VOICE_REC) == 0u);
    env_teardown(&e);
}
/* 10. PCM inválido/duplicado/overflow y missing => restore */
static void test_pcm_rules(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(route_arbiter_pcm_submit(e.arb, 4410, 48000, true, 1, 1) == ROUTE_ERR_INVALID);
    CHECK(route_arbiter_pcm_submit(e.arb, 4410, 44100, false, 2, 1) == ROUTE_ERR_INVALID);
    CHECK(route_arbiter_pcm_submit(e.arb, 4410, 44100, true, 3, 1) == ROUTE_ERR_OK);
    CHECK(route_arbiter_pcm_submit(e.arb, 4410, 44100, true, 3, 2) == ROUTE_ERR_STALE);
    for (uint64_t id = 4; id <= 9; id++) CHECK(route_arbiter_pcm_submit(e.arb, 4410, 44100, true, id, (uint64_t)id) == ROUTE_ERR_OK);
    CHECK(route_arbiter_stats(e.arb)->overflows >= 3);
    CHECK(hb(&e, 20, 0, true) == 0);
    CHECK(enable(&e, 30) == ROUTE_ERR_OK);
    CHECK(route_arbiter_pcm_missing(e.arb, 40) == ROUTE_ERR_OK);
    CHECK(route_arbiter_state(e.arb) == ROUTE_STATE_DEGRADED_BASE);
    env_teardown(&e);
}
/* 11. sink/generated/capture suspenden y restauran */
static void test_suspend_sources(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(hb(&e, 20, 0, true) == 0);
    CHECK(route_arbiter_pcm_submit(e.arb, 4410, 44100, true, 1, 21) == ROUTE_ERR_OK);
    CHECK(enable(&e, 30) == ROUTE_ERR_OK);
    route_arbiter_set_sink_ok(e.arb, false);
    CHECK(route_arbiter_state(e.arb) == ROUTE_STATE_DEGRADED_BASE);
    CHECK((bus_last(&e.fake) & ROUTE_REG_VOICE_REC) == 0);
    CHECK(hb(&e, 40, 0, true) == 0);
    CHECK(route_arbiter_pcm_submit(e.arb, 4410, 44100, true, 2, 41) == ROUTE_ERR_OK);
    route_arbiter_set_sink_ok(e.arb, true);
    CHECK(route_arbiter_set_enabled(e.arb, 50, true) == ROUTE_ERR_OK);
    route_arbiter_set_generated_audio(e.arb, true);
    CHECK(route_arbiter_state(e.arb) == ROUTE_STATE_DEGRADED_BASE);
    env_teardown(&e);
}
/* 12. supervisor: lease expira -> REQUEST_RESTORE; arbitro caído -> FAULT; restaurado -> NONE */
static void test_supervisor(void) {
    route_supervisor_t s; route_supervisor_init(&s, 100, 250);
    route_supervisor_gui_heartbeat(&s, 1000);
    route_supervisor_arbiter_status(&s, 1000, true, ROUTE_STATE_RX_LINUX);
    CHECK(route_supervisor_tick(&s, 1050) == ROUTE_SUP_NONE);
    CHECK(route_supervisor_tick(&s, 1200) == ROUTE_SUP_REQUEST_RESTORE);
    route_supervisor_note_restored(&s, 1230, ROUTE_STATE_DEGRADED_BASE);
    CHECK(s.last_restore_latency_ms == 230);          /* 1200+... desde hb 1000 */
    CHECK(route_supervisor_tick(&s, 1240) == ROUTE_SUP_NONE);
    route_supervisor_arbiter_status(&s, 1300, false, ROUTE_STATE_RX_LINUX);
    CHECK(route_supervisor_tick(&s, 1310) == ROUTE_SUP_FAULT);
}
/* 13. IPC timeout: sin servidor => -2 dentro del bound, no bloqueo indefinido */
static void test_ipc_timeout(void) {
    int fd[2]; CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fd) == 0);
    route_ipc_client_t c; CHECK(route_ipc_client_attach(&c, fd[0], TOKEN) == 0);
    route_ipc_msg_t r; CHECK(route_ipc_client_request(&c, ROUTE_IPC_REQ_STATUS, 0, 0, 1, &r, 100) == -2);
    close(fd[0]); close(fd[1]);
}
/* 14. reinicio del arbitro: shadow inválido hasta boot; no auto-enable */
static void test_restart(void) {
    env_t e; CHECK(env_setup(&e, false) == 0);
    CHECK(enable(&e, 10) == ROUTE_ERR_NOT_READY);
    CHECK(route_arbiter_boot(e.arb) == ROUTE_ERR_OK);
    CHECK(enable(&e, 20) != ROUTE_ERR_OK);
    env_teardown(&e);
}
/* 15. deadlock/concurrencia: 50 requests secuenciales con timeout << */
static void test_serialization(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    for (int i = 0; i < 50; i++) {
        route_ipc_msg_t r; CHECK(req(&e, ROUTE_IPC_REQ_STATUS, 0, 0, (uint64_t)(100+i), &r) == 0);
        CHECK(r.err == ROUTE_ERR_OK);
    }
    CHECK(bus_count(&e.fake) == 1);
    env_teardown(&e);
}
/* 16. volumen/config: escrituras compartidas preservan bits ajenos */
static void test_shared_preserve(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(hb(&e, 20, 0x400u, true) == 0);
    route_ipc_msg_t r; CHECK(req(&e, ROUTE_IPC_REQ_APPLY_SHARED, 0x400u | 0x200000u, 0, 30, &r) == 0);
    CHECK(r.err == ROUTE_ERR_OK);
    CHECK(bus_last(&e.fake) == (0x400u | 0x200000u));
    env_teardown(&e);
}
/* 17. missing PCM bloquea enable */
static void test_missing_pcm(void) {
    env_t e; CHECK(env_setup(&e, true) == 0);
    CHECK(hb(&e, 20, 0, true) == 0);
    CHECK(enable(&e, 30) == ROUTE_ERR_NOT_READY);
    env_teardown(&e);
}

int main(void) {
    test_boot_base();
    test_boot_fail();
    test_enable();
    test_heartbeat_write_fail();
    test_tx_ordering();
    test_tx_bus_fail();
    test_ownership_and_stale();
    test_shared_voice_rejected();
    test_lease_restore();
    test_pcm_rules();
    test_suspend_sources();
    test_supervisor();
    test_ipc_timeout();
    test_restart();
    test_serialization();
    test_shared_preserve();
    test_missing_pcm();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}

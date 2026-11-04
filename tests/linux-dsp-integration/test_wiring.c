#define _DEFAULT_SOURCE
/* QA-LINUX-DSP-INTEGRATION — wiring: worker PCM/control, sink, IPC, QA gate, TTS.
 * Host, mock transport/sink; DEFAULT OFF. */
#define _POSIX_C_SOURCE 200809L
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
#include "route_client_worker.h"
#include "route_wiring.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* ---- fake sink ---- */
typedef struct { atomic_int opens, writes, closes, fail; int16_t last[8]; } sink_t;
static bool s_open(void *ctx, uint32_t rate) { sink_t *s = ctx; atomic_fetch_add(&s->opens, 1); return rate == ROUTE_RATE; }
static int s_write(void *ctx, const int16_t *p, size_t n) { sink_t *s = ctx; if (atomic_load(&s->fail)) return -1; atomic_fetch_add(&s->writes, 1); memcpy(s->last, p, n < 8 ? n * 2 : 8 * 2); return 0; }
static void s_close(void *ctx) { sink_t *s = ctx; atomic_fetch_add(&s->closes, 1); }

static void *producer(void *arg) {
    route_client_worker_t *w = arg;
    int16_t buf[ROUTE_BLOCK_SAMPLES];
    for (int i = 0; i < 200; i++) { memset(buf, i & 0xFF, sizeof(buf)); (void)route_worker_submit_pcm(w, buf, ROUTE_BLOCK_SAMPLES, true, (uint64_t)i + 1); }
    return NULL;
}

/* 1. worker: post-Speex sólo, bounded ring, sink, overflow, generated */
static void test_worker_basic(void) {
    sink_t sk; memset(&sk, 0, sizeof(sk));
    route_sink_t sink = { .ctx = &sk, .open = s_open, .write = s_write, .close = s_close };
    route_client_worker_t *w = route_worker_create(&sink, NULL, 0, 0x6200);
    CHECK(w != NULL);
    int16_t buf[ROUTE_BLOCK_SAMPLES]; memset(buf, 0x11, sizeof(buf));
    CHECK(route_worker_submit_pcm(w, buf, ROUTE_BLOCK_SAMPLES, false, 1) == false);  /* RAW rechazado */
    CHECK(route_worker_submit_pcm(w, buf, ROUTE_BLOCK_SAMPLES, true, 2) == true);
    for (int i = 0; i < 50 && atomic_load(&sk.writes) == 0; i++) usleep(2000);
    CHECK(atomic_load(&sk.opens) >= 1);
    CHECK(atomic_load(&sk.writes) >= 1);
    /* overflow: llenar más que el ring sin que el worker alcance */
    for (int i = 0; i < 20; i++) (void)route_worker_submit_pcm(w, buf, ROUTE_BLOCK_SAMPLES, true, (uint64_t)10 + i);
    uint32_t sub, ov, wr, un, dr;
    usleep(10000);
    route_worker_get_stats(w, &sub, &ov, &wr, &un, &dr);
    CHECK(sub >= 20);
    CHECK(atomic_load(&sk.fail) == 0);
    route_worker_set_generated_audio(w, true);
    CHECK(route_worker_submit_pcm(w, buf, ROUTE_BLOCK_SAMPLES, true, 500) == false); /* TTS priority */
    route_worker_set_generated_audio(w, false);
    route_worker_destroy(w);
    CHECK(atomic_load(&sk.closes) >= 1);
}
/* 2. wiring OFF por defecto: no gate, no RX_LINUX */
static void test_wiring_gate(void) {
    unsetenv("X6200_LINUX_DSP_ROUTE_QA");
    CHECK(route_wiring_qa_gate_ok() == false);
    route_wiring_t *w = route_wiring_create(NULL, NULL, NULL);
    CHECK(w != NULL);
    CHECK(route_wiring_state(w) == ROUTE_WIRING_DISABLED);
    CHECK(route_wiring_set_rx_enabled(w, true) == ROUTE_ERR_NOT_READY);
    route_wiring_submit_pcm(w, NULL, 0, true);   /* no-op seguro */
    route_wiring_set_generated(w, true);
    route_wiring_set_tx(w, true);
    route_wiring_destroy(w);
    setenv("X6200_LINUX_DSP_ROUTE_QA", "1", 1);
    CHECK(route_wiring_qa_gate_ok() == false);   /* falta /etc/x6200_route_qa.enable */
    unsetenv("X6200_LINUX_DSP_ROUTE_QA");
}
/* 3. worker con IPC real: heartbeat/enable/tx; PCM al sink; no double NR */
static uint32_t test_bus_writes;
static int test_bus_write(void *ctx, uint32_t v) { (void)ctx; (void)v; test_bus_writes++; return 0; }
typedef struct {
    sink_t sk;
    route_arbiter_t *arb;
    route_ipc_server_t *srv;
    route_ipc_client_t cli;
    int fd[2];
    pthread_t th;
    atomic_bool stop;
    route_client_worker_t *worker;
} env_t;
static void *srv_thread(void *arg) {
    env_t *e = arg;
    while (!atomic_load(&e->stop)) { int r = route_ipc_server_serve(e->srv, 5); if (r == -1) break; }
    return NULL;
}
static const uint8_t TOKEN[16] = {'R','X','D','P','-','W','I','R','I','N','G','-','0','1','2','3'};
static int env_up(env_t *e) {
    memset(e, 0, sizeof(*e));
    route_bus_t bus = { .write_reg12 = test_bus_write, .ctx = NULL };
    e->arb = route_arbiter_create(&bus, 0);
    CHECK(e->arb != NULL);
    CHECK(route_arbiter_boot(e->arb) == ROUTE_ERR_OK);
    e->srv = route_ipc_server_create(e->arb, TOKEN);
    CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, e->fd) == 0);
    route_ipc_server_attach(e->srv, e->fd[1]);
    route_ipc_client_attach(&e->cli, e->fd[0], TOKEN);
    atomic_init(&e->stop, false);
    pthread_create(&e->th, NULL, srv_thread, e);
    route_sink_t sink = { .ctx = &e->sk, .open = s_open, .write = s_write, .close = s_close };
    e->worker = route_worker_create(&sink, &e->cli, 0, 0x6200);
    CHECK(e->worker != NULL);
    return 0;
}
static void env_down(env_t *e) {
    if (e->worker) route_worker_destroy(e->worker);
    atomic_store(&e->stop, true);
    pthread_join(e->th, NULL);
    close(e->fd[0]);
    route_ipc_server_destroy(e->srv);
    route_arbiter_destroy(e->arb);
}
static void test_worker_ipc(void) {
    env_t e; env_up(&e);
    /* el worker manda heartbeats; el PCM válido lo registra el árbitro directo (test) */
    CHECK(route_arbiter_pcm_submit(e.arb, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 10) == ROUTE_ERR_OK);
    usleep(20000);
    CHECK(route_worker_set_enabled(e.worker, true) == 0);
    CHECK(route_arbiter_state(e.arb) == ROUTE_STATE_RX_LINUX);
    int16_t buf[ROUTE_BLOCK_SAMPLES]; memset(buf, 0x22, sizeof(buf));
    CHECK(route_worker_submit_pcm(e.worker, buf, ROUTE_BLOCK_SAMPLES, true, 2));
    for (int i = 0; i < 100 && atomic_load(&e.sk.writes) == 0; i++) usleep(2000);
    CHECK(atomic_load(&e.sk.writes) >= 1);
    CHECK(route_worker_set_tx(e.worker, true) == 0);   /* disable-first */
    CHECK(route_arbiter_state(e.arb) == ROUTE_STATE_DEGRADED_BASE);
    env_down(&e);
}
/* 4. deadlock: productor y worker concurrentes */
static void test_no_deadlock(void) {
    sink_t sk; memset(&sk, 0, sizeof(sk));
    route_sink_t sink = { .ctx = &sk, .open = s_open, .write = s_write, .close = s_close };
    route_client_worker_t *w = route_worker_create(&sink, NULL, 0, 0);
    CHECK(w != NULL);
    pthread_t p; CHECK(pthread_create(&p, NULL, producer, w) == 0);
    CHECK(pthread_join(p, NULL) == 0);
    usleep(30000);
    uint32_t sub, ov, wr, un, dr;
    route_worker_get_stats(w, &sub, &ov, &wr, &un, &dr);
    CHECK(sub == 200);
    (void)un;
    route_worker_destroy(w);
}
int main(void) {
    test_worker_basic();
    test_wiring_gate();
    test_worker_ipc();
    test_no_deadlock();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}

/* QA-LINUX-DSP-QA-SINK — hook QA con sink real (mock) + IPC. */
#define _DEFAULT_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include "route_arbiter.h"
#include "route_ipc.h"
#include "route_qa_client.h"
#include "route_sink.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
typedef struct { atomic_int writes; } sink_t;
static bool sopen(void *c, uint32_t r) { (void)c; return r == ROUTE_RATE; }
static int swrite(void *c, const int16_t *p, size_t n) { (void)p; (void)n; sink_t *s = c; atomic_fetch_add(&s->writes, 1); return 0; }
static void sclose(void *c) { (void)c; }
typedef struct { int n; uint32_t log[32]; } bus_t;
static int bw(void *c, uint32_t v) { bus_t *b = c; if (b->n < 32) b->log[b->n++] = v; return 0; }
static const uint8_t TOKEN[16] = {'R','X','D','P','-','A','R','B','-','T','O','K','E','N','0','1'};
typedef struct { bus_t b; route_arbiter_t *a; route_ipc_server_t *srv; int lfd, cfd; pthread_t srv_th; atomic_bool stop; char qafile[64], sock[64]; sink_t sk; } env_t;
static void *srvfn(void *arg) { env_t *e = arg; while (!atomic_load(&e->stop)) (void)route_ipc_server_serve(e->srv, 5); return NULL; }
static int up(env_t *e) {
    memset(e, 0, sizeof(*e));
    route_bus_t bus = { .write_reg12 = bw, .ctx = &e->b };
    e->a = route_arbiter_create(&bus, 0); CHECK(e->a != NULL); CHECK(route_arbiter_boot(e->a) == ROUTE_ERR_OK);
    e->srv = route_ipc_server_create(e->a, TOKEN); CHECK(e->srv != NULL);
    snprintf(e->qafile, sizeof(e->qafile), "/tmp/qa-sink-%d.flag", (int)getpid());
    FILE *f = fopen(e->qafile, "w"); if (f) fclose(f);
    snprintf(e->sock, sizeof(e->sock), "/tmp/qa-sink-%d.sock", (int)getpid());
    unlink(e->sock);
    e->lfd = socket(AF_UNIX, SOCK_SEQPACKET, 0); CHECK(e->lfd >= 0);
    struct sockaddr_un sa; memset(&sa, 0, sizeof(sa)); sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, e->sock, sizeof(sa.sun_path)-1);
    CHECK(bind(e->lfd, (struct sockaddr *)&sa, sizeof(sa)) == 0);
    CHECK(listen(e->lfd, 1) == 0);
    setenv("X6200_LINUX_DSP_ROUTE_QA", "1", 1);
    setenv("X6200_ROUTE_QA_FILE", e->qafile, 1);
    setenv("X6200_ROUTE_QA_IPC", e->sock, 1);
    return 0;
}
static int accept_attach(env_t *e) {
    e->cfd = accept(e->lfd, NULL, NULL); CHECK(e->cfd >= 0);
    route_ipc_server_attach(e->srv, e->cfd);
    atomic_init(&e->stop, false); pthread_create(&e->srv_th, NULL, srvfn, e);
    return e->cfd;
}
static void down(env_t *e) {
    x6200_route_qa_stop(); unsetenv("X6200_LINUX_DSP_ROUTE_QA"); unsetenv("X6200_ROUTE_QA_FILE"); unsetenv("X6200_ROUTE_QA_IPC");
    if (e->cfd > 0) { atomic_store(&e->stop, true); pthread_join(e->srv_th, NULL); close(e->cfd); }
    close(e->lfd); unlink(e->sock); unlink(e->qafile);
    route_ipc_server_destroy(e->srv); route_arbiter_destroy(e->a);
}
static void pcm(bool processed) { int16_t buf[ROUTE_BLOCK_SAMPLES]; memset(buf, 3, sizeof(buf)); x6200_route_qa_submit_pcm(buf, ROUTE_BLOCK_SAMPLES, processed); }
static void test_gate_off_and_closed(void) {
    env_t e; up(&e);
    unsetenv("X6200_LINUX_DSP_ROUTE_QA");
    route_sink_t mock = { .ctx = &e.sk, .open = sopen, .write = swrite, .close = sclose };
    CHECK(x6200_route_qa_start(&mock) != 0);
    CHECK(x6200_route_qa_state() != ROUTE_QA_ACTIVE);
    CHECK(e.b.n == 1);   /* sólo boot BASE */
    setenv("X6200_LINUX_DSP_ROUTE_QA", "1", 1);
    down(&e);
    /* IPC inexistente: falla cerrado, sin writes extra */
    env_t e2; up(&e2);
    setenv("X6200_ROUTE_QA_IPC", "/tmp/no-such-qa-sock", 1);
    CHECK(x6200_route_qa_start(&mock) != 0);
    CHECK(x6200_route_qa_state() != ROUTE_QA_ACTIVE);
    CHECK(e2.b.n == 1);
    down(&e2);
}
static void test_valid_activation(void) {
    env_t e; up(&e);
    route_sink_t mock = { .ctx = &e.sk, .open = sopen, .write = swrite, .close = sclose };
    CHECK(x6200_route_qa_start(&mock) == 0);
    CHECK(accept_attach(&e) >= 0);
    CHECK(x6200_route_qa_state() == ROUTE_QA_READY);
    /* sin PCM post-Speex no activa */
    CHECK(x6200_route_qa_activate_now() != 0);
    CHECK(route_arbiter_state(e.a) != ROUTE_STATE_RX_LINUX);
    /* RAW (no post-Speex) no cuenta */
    pcm(false);
    CHECK(x6200_route_qa_activate_now() != 0);
    pcm(true);
    CHECK(x6200_route_qa_activate_now() == 0);
    CHECK(route_arbiter_state(e.a) == ROUTE_STATE_RX_LINUX);
    CHECK(x6200_route_qa_state() == ROUTE_QA_ACTIVE);
    CHECK((e.b.log[e.b.n-1] & ROUTE_REG_VOICE_REC) != 0);
    /* sink recibe PCM (worker), callback no bloquea */
    for (int i = 0; i < 100 && atomic_load(&e.sk.writes) == 0; i++) usleep(2000);
    CHECK(atomic_load(&e.sk.writes) >= 1);
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < 100; i++) pcm(true);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long us = (t1.tv_sec-t0.tv_sec)*1000000L + (t1.tv_nsec-t0.tv_nsec)/1000L;
    CHECK(us < 200000);   /* 100 submits << 200 ms: no IPC en callback */
    /* generated deniega reactivación */
    x6200_route_qa_generated(true);
    CHECK(1);
    x6200_route_qa_generated(false);
    down(&e);
}
int main(void) { test_gate_off_and_closed(); test_valid_activation(); printf("%d comprobaciones, %d fallos\n", checks, fails); return fails ? 1 : 0; }

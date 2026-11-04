/* QA-LINUX-DSP-QA-ENABLE — activación RX_LINUX QA-only por IPC real (host). */
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
#include "route_sink.h"
#include "route_wiring.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
typedef struct { int opens, writes; } sink_t;
static bool sk_open(void *c, uint32_t r) { sink_t *s = c; s->opens++; return r == ROUTE_RATE; }
static int sk_write(void *c, const int16_t *p, size_t n) { sink_t *s = c; (void)p; (void)n; s->writes++; return 0; }
static void sk_close(void *c) { (void)c; }
typedef struct { int writes; uint32_t log[64]; } bus_t;
static int bw(void *c, uint32_t v) { bus_t *b = c; if (b->writes < 64) b->log[b->writes++] = v; return 0; }
static const uint8_t TOKEN[16] = {'R','X','D','P','-','Q','A','-','T','O','K','E','N','0','1','2'};
typedef struct { bus_t b; route_arbiter_t *a; route_ipc_server_t *srv; int fd[2]; pthread_t th; atomic_bool stop; route_wiring_t *w; sink_t sk; char qafile[64]; } env_t;
static void *srvfn(void *arg) { env_t *e = arg; while (!atomic_load(&e->stop)) { int r = route_ipc_server_serve(e->srv, 5); if (r == -1) break; } return NULL; }
static int up(env_t *e) {
    memset(e, 0, sizeof(*e));
    route_bus_t bus = { .write_reg12 = bw, .ctx = &e->b };
    e->a = route_arbiter_create(&bus, 0); CHECK(e->a != NULL); CHECK(route_arbiter_boot(e->a) == ROUTE_ERR_OK);
    e->srv = route_ipc_server_create(e->a, TOKEN); CHECK(e->srv != NULL);
    CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, e->fd) == 0);
    route_ipc_server_attach(e->srv, e->fd[1]);
    atomic_init(&e->stop, false); pthread_create(&e->th, NULL, srvfn, e);
    snprintf(e->qafile, sizeof(e->qafile), "/tmp/qa-enable-%d.flag", (int)getpid());
    FILE *f = fopen(e->qafile, "w"); if (f) fclose(f);
    setenv("X6200_LINUX_DSP_ROUTE_QA", "1", 1);
    setenv("X6200_ROUTE_QA_FILE", e->qafile, 1);
    route_sink_t sink = { .ctx = &e->sk, .open = sk_open, .write = sk_write, .close = sk_close };
    e->w = route_wiring_create(&sink, "/tmp/qa.enable", TOKEN);
    CHECK(e->w != NULL);
    CHECK(route_wiring_attach_ipc(e->w, e->fd[0]) == 0);
    route_wiring_set_current_reg(e->w, 0x100000u);
    return 0;
}
static void down(env_t *e) {
    route_wiring_destroy(e->w); atomic_store(&e->stop, true); pthread_join(e->th, NULL);
    close(e->fd[0]); route_ipc_server_destroy(e->srv); route_arbiter_destroy(e->a);
    unlink(e->qafile);
}
static void pcm(route_wiring_t *w) { int16_t buf[ROUTE_BLOCK_SAMPLES]; memset(buf, 1, sizeof(buf)); route_wiring_submit_pcm(w, buf, ROUTE_BLOCK_SAMPLES, true); }
static void test_gate_and_preconditions(void) {
    env_t e; up(&e);
    /* gate runtime ausente => no activa */
    unlink(e.qafile);
    pcm(e.w);
    CHECK(route_wiring_set_rx_enabled(e.w, true) == ROUTE_ERR_NOT_READY);
    CHECK(route_arbiter_state(e.a) != ROUTE_STATE_RX_LINUX);
    FILE *f = fopen(e.qafile, "w"); if (f) fclose(f);
    /* sin PCM post-Speex => no activa */
    route_wiring_t *w2 = route_wiring_create(&(route_sink_t){0}, "/tmp/qa.enable", TOKEN);
    CHECK(route_wiring_attach_ipc(w2, dup(e.fd[0])) == 0);
    CHECK(route_wiring_set_rx_enabled(w2, true) == ROUTE_ERR_NOT_READY);
    route_wiring_destroy(w2);
    /* generated/TX deniegan */
    pcm(e.w);
    route_wiring_set_generated(e.w, true);
    CHECK(route_wiring_set_rx_enabled(e.w, true) == ROUTE_ERR_NOT_READY);
    route_wiring_set_generated(e.w, false);
    route_wiring_set_current_reg(e.w, 0x100000u | ROUTE_REG_IPTT);
    CHECK(route_wiring_set_rx_enabled(e.w, true) == ROUTE_ERR_TX_ACTIVE);
    route_wiring_set_current_reg(e.w, 0x100000u);
    down(&e);
}
static void test_valid_activation(void) {
    env_t e; up(&e);
    pcm(e.w);
    CHECK(route_wiring_set_rx_enabled(e.w, true) == ROUTE_ERR_OK);
    CHECK(route_wiring_enabled(e.w));
    CHECK(route_wiring_state(e.w) == ROUTE_WIRING_ACTIVE);
    CHECK(route_arbiter_state(e.a) == ROUTE_STATE_RX_LINUX);
    CHECK(e.b.writes >= 2);   /* boot + enable (un solo writer: árbitro) */
    CHECK((e.b.log[e.b.writes-1] & ROUTE_REG_VOICE_REC) != 0);
    CHECK(route_wiring_set_rx_enabled(e.w, false) == ROUTE_ERR_OK);
    for (int i = 0; i < 100 && route_arbiter_state(e.a) == ROUTE_STATE_RX_LINUX; i++) usleep(2000);
    CHECK(route_arbiter_state(e.a) == ROUTE_STATE_BASE);
    unsetenv("X6200_LINUX_DSP_ROUTE_QA");
    CHECK(route_wiring_set_rx_enabled(e.w, true) == ROUTE_ERR_NOT_READY);
    down(&e);
}
int main(void) {
    test_gate_and_preconditions();
    test_valid_activation();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}

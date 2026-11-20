/* Worker de PCM/control del lado GUI-router. Callback de audio: sólo SPSC submit. */
#define _POSIX_C_SOURCE 200809L
#include "route_client_worker.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NWORK_BLOCKS ROUTE_RING_BLOCKS
#define WBLOCK ROUTE_BLOCK_SAMPLES

struct route_client_worker {
    route_sink_t sink;
    route_ipc_client_t *ipc;
    pthread_t th;
    pthread_mutex_t mu;
    atomic_bool running, stop, generated;
    int16_t ring[NWORK_BLOCKS][WBLOCK];
    unsigned head, count;
    atomic_uint submitted, overflow, written, underruns, dropped;
    bool opened, sink_ok;
    uint32_t pid;
    uint64_t last_write_ms;
};

static uint64_t now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static void *worker_main(void *arg) {
    route_client_worker_t *w = arg;
    while (!atomic_load(&w->stop)) {
        int16_t out[WBLOCK];
        bool have = false;
        pthread_mutex_lock(&w->mu);
        if (w->count > 0) {
            memcpy(out, w->ring[w->head], sizeof(out));
            w->head = (w->head + 1u) % NWORK_BLOCKS;
            w->count--;
            have = true;
        }
        bool gen = atomic_load(&w->generated);
        pthread_mutex_unlock(&w->mu);
        if (have && !gen) {
            if (!w->opened) {
                w->opened = (w->sink.open != NULL) && w->sink.open(w->sink.ctx, ROUTE_RATE);
            }
            if (w->opened && w->sink.write != NULL) {
                int r = w->sink.write(w->sink.ctx, out, WBLOCK);
                if (r == 0) { atomic_fetch_add(&w->written, 1); w->sink_ok = true; w->last_write_ms = now_ms(); }
                else if (r > 0) { atomic_fetch_add(&w->underruns, 1); }
                else {
                    /* sink caído: cerrar y dejar que el control/arbitro degrade */
                    if (w->sink.close) w->sink.close(w->sink.ctx);
                    w->opened = false; w->sink_ok = false;
                    atomic_fetch_add(&w->underruns, 1);
                }
            }
        } else if (!have) {
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 2 * 1000000L };
            nanosleep(&ts, NULL);
        }
        /* Control no bloqueante: sólo si hay IPC y no está en generated/TX. */
        if (w->ipc != NULL && !gen) {
            route_ipc_msg_t resp;
            (void)route_ipc_client_request(w->ipc, ROUTE_IPC_REQ_HEARTBEAT, 0, 1, now_ms(), &resp, 5);
        }
    }
    if (w->opened && w->sink.close != NULL) w->sink.close(w->sink.ctx);
    return NULL;
}

route_client_worker_t *route_worker_create(const route_sink_t *sink, route_ipc_client_t *ipc,
                                           uint64_t now_ms0, uint32_t token_pid) {
    (void)now_ms0;
    route_client_worker_t *w = calloc(1, sizeof(*w));
    if (w == NULL) return NULL;
    if (sink != NULL) w->sink = *sink;
    w->ipc = ipc; w->pid = token_pid;
    pthread_mutex_init(&w->mu, NULL);
    atomic_init(&w->running, true); atomic_init(&w->stop, false); atomic_init(&w->generated, false);
    atomic_init(&w->submitted, 0); atomic_init(&w->overflow, 0); atomic_init(&w->written, 0);
    atomic_init(&w->underruns, 0); atomic_init(&w->dropped, 0);
    if (pthread_create(&w->th, NULL, worker_main, w) != 0) { free(w); return NULL; }
    return w;
}
void route_worker_destroy(route_client_worker_t *w) {
    if (w == NULL) return;
    atomic_store(&w->stop, true);
    pthread_join(w->th, NULL);
    if (w->opened && w->sink.close != NULL) w->sink.close(w->sink.ctx);
    pthread_mutex_destroy(&w->mu);
    free(w);
}
bool route_worker_submit_pcm(route_client_worker_t *w, const int16_t *samples, size_t count,
                             bool already_processed, uint64_t block_id) {
    (void)block_id;
    if (w == NULL || samples == NULL || count == 0 || count > WBLOCK) return false;
    if (!already_processed) return false;  /* sólo post-SpeexDSP, sin doble procesamiento */
    if (atomic_load(&w->generated)) return false;
    bool ok = true;
    pthread_mutex_lock(&w->mu);
    if (w->count == NWORK_BLOCKS) { w->head = (w->head + 1u) % NWORK_BLOCKS; w->count--; atomic_fetch_add(&w->overflow, 1); ok = false; }
    memcpy(w->ring[(w->head + w->count) % NWORK_BLOCKS], samples, count * sizeof(int16_t));
    if (count < WBLOCK) memset(w->ring[(w->head + w->count) % NWORK_BLOCKS] + count, 0, (WBLOCK - count) * sizeof(int16_t));
    w->count++;
    pthread_mutex_unlock(&w->mu);
    atomic_fetch_add(&w->submitted, 1);
    return ok;
}
void route_worker_set_generated_audio(route_client_worker_t *w, bool active) {
    if (w == NULL) return;
    atomic_store(&w->generated, active);
    if (active) {
        pthread_mutex_lock(&w->mu);
        w->count = 0; w->head = 0;
        pthread_mutex_unlock(&w->mu);
    }
}
int route_worker_set_tx(route_client_worker_t *w, bool tx) {
    if (w == NULL || w->ipc == NULL) return -1;
    route_ipc_msg_t r;
    if (tx) return route_ipc_client_request(w->ipc, ROUTE_IPC_REQ_TX_PREPARE, 0, 0, now_ms(), &r, 100) == 0 ? 0 : -1;
    return route_ipc_client_request(w->ipc, ROUTE_IPC_REQ_DISABLE, 0, 0, now_ms(), &r, 100) == 0 ? 0 : -1;
}
int route_worker_set_enabled(route_client_worker_t *w, bool enabled) {
    if (w == NULL || w->ipc == NULL) return -1;
    route_ipc_msg_t r;
    return route_ipc_client_request(w->ipc, enabled ? ROUTE_IPC_REQ_ENABLE : ROUTE_IPC_REQ_DISABLE,
                                    0, 0, now_ms(), &r, 100) == 0 ? 0 : -1;
}
int route_worker_pump_control(route_client_worker_t *w, int timeout_ms) {
    if (w == NULL || w->ipc == NULL) return -1;
    route_ipc_msg_t r;
    if (route_ipc_client_request(w->ipc, ROUTE_IPC_REQ_STATUS, 0, 0, now_ms(), &r, timeout_ms) != 0) return -1;
    return (int)r.err;
}
void route_worker_get_stats(route_client_worker_t *w, uint32_t *submitted, uint32_t *overflow,
                            uint32_t *written, uint32_t *underruns, uint32_t *dropped) {
    if (w == NULL) return;
    if (submitted) *submitted = atomic_load(&w->submitted);
    if (overflow) *overflow = atomic_load(&w->overflow);
    if (written) *written = atomic_load(&w->written);
    if (underruns) *underruns = atomic_load(&w->underruns);
    if (dropped) *dropped = atomic_load(&w->dropped);
}
size_t route_worker_pending(const route_client_worker_t *w) {
    if (w == NULL) return 0;
    pthread_mutex_t *m = (pthread_mutex_t *)&w->mu;
    pthread_mutex_lock(m);
    size_t n = w->count;
    pthread_mutex_unlock(m);
    return n;
}
bool route_worker_ready(const route_client_worker_t *w) { return w != NULL && w->sink_ok; }

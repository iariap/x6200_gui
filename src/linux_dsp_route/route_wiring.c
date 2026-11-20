#define _POSIX_C_SOURCE 200809L
#include "route_wiring.h"
#include "route_client_worker.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#ifdef X6200_LINUX_DSP_ROUTE
/* Transport adapter (sólo cuando la feature está compilada ON). */
int route_transport_x6200_write_reg12(void *ctx, uint32_t value);
#endif

struct route_wiring {
    route_wiring_state_t state;
    uint32_t current_reg;
    bool pcm_fresh;
    uint32_t pcm_seq;
    bool generated;
    route_sink_t sink;
    char ipc_path[108];
    uint8_t token[ROUTE_IPC_TOKEN_LEN];
    route_ipc_client_t ipc;
    route_client_worker_t *worker;
    int ipc_fd;
    bool enabled;
};

bool route_wiring_qa_gate_ok(void) {
    const char *e = getenv("X6200_LINUX_DSP_ROUTE_QA");
    if (e == NULL || strcmp(e, "1") != 0) return false;
    const char *f = getenv("X6200_ROUTE_QA_FILE");
    return access(f != NULL ? f : "/etc/x6200_route_qa.enable", R_OK) == 0;
}

route_wiring_t *route_wiring_create(const route_sink_t *sink, const char *ipc_path,
                                    const uint8_t token[ROUTE_IPC_TOKEN_LEN]) {
    route_wiring_t *w = calloc(1, sizeof(*w));
    if (w == NULL) return NULL;
    if (sink != NULL) w->sink = *sink;
    if (ipc_path != NULL) { strncpy(w->ipc_path, ipc_path, sizeof(w->ipc_path) - 1); }
    if (token != NULL) memcpy(w->token, token, ROUTE_IPC_TOKEN_LEN);
    w->ipc_fd = -1; w->enabled = false; w->current_reg = 0; w->pcm_fresh = false;
    if (ipc_path != NULL) w->state = ROUTE_WIRING_READY; else w->state = ROUTE_WIRING_DISABLED;
    return w;
}
int route_wiring_attach_ipc(route_wiring_t *w, int fd) {
    if (w == NULL || fd < 0) return -1;
    if (w->worker != NULL) route_worker_destroy(w->worker);
    route_ipc_client_attach(&w->ipc, fd, w->token);
    w->ipc_fd = fd;
    w->worker = route_worker_create(&w->sink, &w->ipc, 0, 0);
    if (w->worker == NULL) return -1;
    w->state = ROUTE_WIRING_READY;
    return 0;
}
void route_wiring_set_current_reg(route_wiring_t *w, uint32_t value) {
    if (w) { w->current_reg = value & ~ROUTE_REG_VOICE_REC; }
}
bool route_wiring_has_fresh_pcm(const route_wiring_t *w) { return w != NULL && w->pcm_fresh; }
void route_wiring_destroy(route_wiring_t *w) {
    if (w == NULL) return;
    if (w->worker != NULL) route_worker_destroy(w->worker);
    if (w->ipc_fd >= 0) close(w->ipc_fd);
    free(w);
}
route_wiring_state_t route_wiring_state(const route_wiring_t *w) { return w ? w->state : ROUTE_WIRING_DISABLED; }
bool route_wiring_enabled(const route_wiring_t *w) { return w != NULL && w->enabled; }

void route_wiring_submit_pcm(route_wiring_t *w, const int16_t *samples, size_t count,
                             bool already_processed) {
    if (w == NULL || w->worker == NULL || !already_processed) return;
    if (route_worker_submit_pcm(w->worker, samples, count, true, 0)) {
        w->pcm_fresh = true; w->pcm_seq++;
    }
}
void route_wiring_set_generated(route_wiring_t *w, bool active) {
    if (w == NULL) return;
    w->generated = active;
    if (w->worker != NULL) route_worker_set_generated_audio(w->worker, active);
}
int route_wiring_set_tx(route_wiring_t *w, bool tx) {
    if (w == NULL || w->worker == NULL) return -1;
    return route_worker_set_tx(w->worker, tx);
}
route_err_t route_wiring_set_rx_enabled(route_wiring_t *w, bool enabled) {
    if (w == NULL) return ROUTE_ERR_INVALID;
    if (!enabled) {
        if (w->worker != NULL) (void)route_worker_set_enabled(w->worker, false);
        w->enabled = false;
        if (w->state == ROUTE_WIRING_ACTIVE) w->state = ROUTE_WIRING_READY;
        return ROUTE_ERR_OK;
    }
    /* QA-only, sin auto-enable: exige gate compile+runtime y cliente/worker/IPC reales. */
    if (w->state == ROUTE_WIRING_DISABLED) return ROUTE_ERR_NOT_READY;
    if (!route_wiring_qa_gate_ok()) return ROUTE_ERR_NOT_READY;
    if (w->worker == NULL || w->ipc_fd < 0) return ROUTE_ERR_NOT_READY;
    if (w->generated) return ROUTE_ERR_NOT_READY;                     /* TTS/generado */
    if (!w->pcm_fresh) return ROUTE_ERR_NOT_READY;                    /* PCM post-Speex ausente */
    if ((w->current_reg & ROUTE_TX_ACTIVITY_MASK) != 0) return ROUTE_ERR_TX_ACTIVE;
    struct timespec ts; uint64_t now = 0;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        now = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    /* Envía PCM_READY (bloque real ya en el worker) y heartbeat con el valor
     * actual del reg12; luego pide ENABLE al árbitro (single owner, sin bypass). */
    if (route_worker_send_pcm_ready(w->worker, w->pcm_seq ? w->pcm_seq : 1, now) != 0) return ROUTE_ERR_TIMEOUT;
    route_ipc_msg_t r;
    if (route_ipc_client_request(&w->ipc, ROUTE_IPC_REQ_HEARTBEAT, w->current_reg, 1, now, &r, 100) != 0)
        return ROUTE_ERR_TIMEOUT;
    if (r.err != ROUTE_ERR_OK) { w->state = ROUTE_WIRING_BLOCKED; return (route_err_t)r.err; }
    int ee = route_worker_set_enabled(w->worker, true);
    if (ee == -1) return ROUTE_ERR_TIMEOUT;
    if (ee != 0) { w->state = ROUTE_WIRING_BLOCKED; return (route_err_t)ee; }
    w->enabled = true; w->state = ROUTE_WIRING_ACTIVE;
    return ROUTE_ERR_OK;
}
void route_wiring_pump(route_wiring_t *w) {
    if (w == NULL || w->worker == NULL) return;
    (void)route_worker_pump_control(w->worker, 10);
}
void route_wiring_get_stats(const route_wiring_t *w, uint32_t *submitted, uint32_t *overflow,
                            uint32_t *written, uint32_t *underruns) {
    if (w == NULL || w->worker == NULL) return;
    uint32_t dropped;
    route_worker_get_stats(w->worker, submitted, overflow, written, underruns, &dropped);
}

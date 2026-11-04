#ifdef X6200_LINUX_DSP_ROUTE
#define _POSIX_C_SOURCE 200809L
#include "route_qa_client.h"
#include "route_wiring.h"
#include "route_pulse_sink.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static void qa_log(const char *m) { fprintf(stderr, "x6200 QA: %s\n", m); fflush(stderr); }

#ifdef ROUTE_QA_TEST_NO_PULSE
/* Build host de tests: sin PulseAudio, falla cerrado. */
route_sink_t route_pulse_sink_create(void) { route_sink_t s = {0}; return s; }
void route_pulse_sink_destroy(route_sink_t *sink) { (void)sink; }
#endif

#define QA_TOKEN_DEFAULT {'R','X','D','P','-','A','R','B','-','T','O','K','E','N','0','1'}
static route_wiring_t *g_w;
static route_qa_state_t g_state = ROUTE_QA_DISABLED;
static int g_err = ROUTE_ERR_NOT_READY;
static volatile int g_stop;
static pthread_t g_activator;
static bool g_activator_started;
static route_sink_t g_sink;
static bool g_sink_owned;

static void *activate_thread(void *arg) {
    (void)arg;
    /* Espera PCM post-Speex real antes de pedir activación (sin auto-enable). */
    for (int i = 0; i < 250 && !g_stop; i++) {
        if (g_w != NULL && route_wiring_has_fresh_pcm(g_w)) break;
        struct timespec ts = {0, 20*1000*1000L}; nanosleep(&ts, NULL);
    }
    if (!g_stop && g_w != NULL && route_wiring_has_fresh_pcm(g_w)) {
        qa_log("PCM post-Speex fresco; pidiendo ENABLE al arbitro");
        route_err_t e = route_wiring_set_rx_enabled(g_w, true);
        g_err = e;
        g_state = (e == ROUTE_ERR_OK) ? ROUTE_QA_ACTIVE : ROUTE_QA_BLOCKED;
        fprintf(stderr, "x6200 QA: activation err=%d state=%s\n",
                (int)e, g_state == ROUTE_QA_ACTIVE ? "ACTIVE" : "BLOCKED");
        fflush(stderr);
    }
    return NULL;
}
static int start_with(const route_sink_t *sink) {
    if (g_w != NULL) return 0;
    if (!route_wiring_qa_gate_ok()) { g_err = ROUTE_ERR_NOT_READY; g_state = ROUTE_QA_DISABLED; qa_log("gate off/archivo ausente -> no activa"); return -1; }
    const char *path = getenv("X6200_ROUTE_QA_IPC");
    if (sink == NULL || path == NULL) { g_err = ROUTE_ERR_NOT_READY; g_state = ROUTE_QA_BLOCKED; qa_log("sin sink/ipc_path -> falla cerrado"); return -1; }
    fprintf(stderr, "x6200 QA: gate OK ipc=%s\n", path); fflush(stderr);
    const char *tok_env = getenv("X6200_ROUTE_QA_TOKEN");
    uint8_t token[ROUTE_IPC_TOKEN_LEN];
    if (tok_env != NULL && strlen(tok_env) == ROUTE_IPC_TOKEN_LEN) memcpy(token, tok_env, ROUTE_IPC_TOKEN_LEN);
    else { static const uint8_t def[ROUTE_IPC_TOKEN_LEN] = QA_TOKEN_DEFAULT; memcpy(token, def, ROUTE_IPC_TOKEN_LEN); }
    g_w = route_wiring_create(sink, path, token);
    if (g_w == NULL) { g_err = ROUTE_ERR_INVALID; g_state = ROUTE_QA_BLOCKED; return -1; }
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0) { g_err = ROUTE_ERR_TIMEOUT; g_state = ROUTE_QA_BLOCKED; return -1; }
    struct sockaddr_un sa; memset(&sa, 0, sizeof(sa)); sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, path, sizeof(sa.sun_path)-1);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        close(fd); g_err = ROUTE_ERR_TIMEOUT; g_state = ROUTE_QA_BLOCKED;
        fprintf(stderr, "x6200 QA client: IPC no disponible en %s (falla cerrado)\n", path);
        return -1;
    }
    if (route_wiring_attach_ipc(g_w, fd) != 0) { g_err = ROUTE_ERR_INVALID; g_state = ROUTE_QA_BLOCKED; qa_log("attach IPC fallo"); return -1; }
    qa_log("IPC conectado, wiring READY (sin activar)");
    g_state = ROUTE_QA_READY; g_err = ROUTE_ERR_OK;
    return 0;
}
int x6200_route_qa_start_default(void) {
#ifdef ROUTE_QA_TEST_NO_PULSE
    return -1;   /* build host de tests: sin sink real, falla cerrado */
#else
    g_sink = route_pulse_sink_create(); g_sink_owned = true;
    if (g_sink.ctx == NULL) { g_err = ROUTE_ERR_NOT_READY; g_state = ROUTE_QA_BLOCKED; qa_log("PulseAudio sink no disponible"); return -1; }
    int r = start_with(&g_sink);
    if (r == 0 && getenv("X6200_ROUTE_QA_ACTIVATE") != NULL && strcmp(getenv("X6200_ROUTE_QA_ACTIVATE"), "1") == 0) {
        if (!g_activator_started) { g_activator_started = true; pthread_create(&g_activator, NULL, activate_thread, NULL); pthread_detach(g_activator); }
    }
    return r;
#endif
}
int x6200_route_qa_start(const route_sink_t *sink) {
    int r = start_with(sink);
    if (r == 0 && getenv("X6200_ROUTE_QA_ACTIVATE") != NULL && strcmp(getenv("X6200_ROUTE_QA_ACTIVATE"), "1") == 0) {
        if (!g_activator_started) { g_activator_started = true; pthread_create(&g_activator, NULL, activate_thread, NULL); pthread_detach(g_activator); }
    }
    return r;
}
int x6200_route_qa_attach_ipc(int fd) { if (g_w == NULL) return -1; return route_wiring_attach_ipc(g_w, fd); }
void x6200_route_qa_submit_pcm(const int16_t *samples, size_t count, bool processed) { if (g_w) route_wiring_submit_pcm(g_w, samples, count, processed); }
void x6200_route_qa_generated(bool active) { if (g_w) route_wiring_set_generated(g_w, active); }
void x6200_route_qa_tx(bool tx) { if (g_w) (void)route_wiring_set_tx(g_w, tx); }
int x6200_route_qa_activate_now(void) {
    if (g_w == NULL) { g_err = ROUTE_ERR_NOT_READY; return -1; }
    qa_log("activación manual QA");
    route_err_t e = route_wiring_set_rx_enabled(g_w, true);
    g_err = e; g_state = (e == ROUTE_ERR_OK) ? ROUTE_QA_ACTIVE : ROUTE_QA_BLOCKED;
    return e == ROUTE_ERR_OK ? 0 : -1;
}
route_qa_state_t x6200_route_qa_state(void) { return g_state; }
int x6200_route_qa_last_error(void) { return g_err; }
void x6200_route_qa_stop(void) {
    qa_log("stop QA: disable + destroy");
    g_stop = 1;
    if (g_w != NULL) { (void)route_wiring_set_rx_enabled(g_w, false); route_wiring_destroy(g_w); g_w = NULL; }
    if (g_sink_owned) { route_pulse_sink_destroy(&g_sink); g_sink_owned = false; }
    g_state = ROUTE_QA_DISABLED; g_err = ROUTE_ERR_OK;
}
#endif

#pragma once
#include "route_common.h"
#include "route_sink.h"
#include "route_ipc.h"

typedef struct route_client_worker route_client_worker_t;

/* El callback de captura llama submit (SPSC, sin malloc/bloqueo/copias grandes no acotadas).
 * El worker dedicado abre/escribe el sink y envía control (heartbeat/flags) por IPC con timeouts. */
route_client_worker_t *route_worker_create(const route_sink_t *sink, route_ipc_client_t *ipc,
                                           uint64_t now_ms, uint32_t token_pid);
void route_worker_destroy(route_client_worker_t *w);
/* true si aceptado; false si deshabilitado/overflow/argumentos invalidos (no bloquea). */
bool route_worker_submit_pcm(route_client_worker_t *w, const int16_t *samples, size_t count,
                             bool already_processed, uint64_t block_id);
void route_worker_set_generated_audio(route_client_worker_t *w, bool active);  /* TTS/playback */
int route_worker_set_tx(route_client_worker_t *w, bool tx);                    /* control IPC */
int route_worker_set_enabled(route_client_worker_t *w, bool enabled);          /* control IPC */
int route_worker_pump_control(route_client_worker_t *w, int timeout_ms);
/* Envía PCM_READY (post-Speex vivo) al árbitro. Ruta de control, nunca callback. */
int route_worker_send_pcm_ready(route_client_worker_t *w, uint32_t block_id, uint64_t now_ms);       /* worker/tick */
void route_worker_get_stats(route_client_worker_t *w, uint32_t *submitted, uint32_t *overflow,
                            uint32_t *written, uint32_t *underruns, uint32_t *dropped);
size_t route_worker_pending(const route_client_worker_t *w);
bool route_worker_ready(const route_client_worker_t *w);

#pragma once
#include "route_common.h"
#include "route_sink.h"
#include "route_ipc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Wiring branch-local, DEFAULT OFF. Compilado sólo con X6200_LINUX_DSP_ROUTE=ON.
 * QA gate: compile-time + runtime (env X6200_LINUX_DSP_ROUTE_QA=1 y archivo
 * /etc/x6200_route_qa.enable). NO operator-accessible, no UI/settings.
 *
 * LIMITACIÓN DE SEGURIDAD (bloqueante para RX_LINUX): el proxy completo de TODAS
 * las escrituras de libaether (PTT/ATU/resto) hacia el árbitro NO está cableado en
 * esta etapa; mientras GUI y árbitro puedan escribir el registro 12 en paralelo,
 * la activación RX_LINUX se mantiene BLOQUEADA. Las funciones existen y se prueban
 * con mocks, pero route_wiring_set_rx_enabled(true) devuelve false por diseño. */

typedef enum {
    ROUTE_WIRING_DISABLED = 0,     /* feature OFF o QA gate no habilitado */
    ROUTE_WIRING_READY,            /* inicializado, RX_LINUX aún bloqueado */
    ROUTE_WIRING_BLOCKED,          /* habilitado QA pero proxy incompleto: bloqueado */
    ROUTE_WIRING_FAULT,
} route_wiring_state_t;

typedef struct route_wiring route_wiring_t;

/* sink y control backend inyectados; ipc_path = socket del árbitro (NULL en host/no activado). */
route_wiring_t *route_wiring_create(const route_sink_t *sink, const char *ipc_path,
                                    const uint8_t token[ROUTE_IPC_TOKEN_LEN]);
void route_wiring_destroy(route_wiring_t *w);
route_wiring_state_t route_wiring_state(const route_wiring_t *w);
bool route_wiring_enabled(const route_wiring_t *w);
/* Llamado desde capture_deliver() con el buffer YA post-SpeexDSP; no bloquea. */
void route_wiring_submit_pcm(route_wiring_t *w, const int16_t *samples, size_t count,
                             bool already_processed);
/* TTS/audio generado (audio_play_en) tiene prioridad: suspende el router. */
void route_wiring_set_generated(route_wiring_t *w, bool active);
/* TX/PTT/ATU: disable-first vía IPC antes de permitir; devuelve 0 si seguro. */
int  route_wiring_set_tx(route_wiring_t *w, bool tx);
/* Activación QA: sólo con gate ok; mientras el proxy no esté completo devuelve
 * ROUTE_ERR_OWNERSHIP (bloqueado). Nunca auto-enable. */
route_err_t route_wiring_set_rx_enabled(route_wiring_t *w, bool enabled);
void route_wiring_pump(route_wiring_t *w);
void route_wiring_get_stats(const route_wiring_t *w, uint32_t *submitted, uint32_t *overflow,
                            uint32_t *written, uint32_t *underruns);
bool route_wiring_qa_gate_ok(void);

#pragma once
#include "route_sink.h"
#include <stdbool.h>
#include <stdint.h>
/* Cliente QA de la ruta Linux-DSP: crea route_wiring con sink real, IPC/token del
 * servicio QA y activa RX_LINUX SÓLO con gate + env explícito. Default OFF.
 * Contrato de lanzamiento (ver docs/qa-sink-launch.md):
 *   X6200_LINUX_DSP_ROUTE_QA=1
 *   /etc/x6200_route_qa.enable
 *   X6200_ROUTE_QA_IPC=/run/x6200_route.sock
 *   X6200_ROUTE_QA_TOKEN=<16 bytes>  (opcional; default token QA)
 *   X6200_ROUTE_QA_ACTIVATE=1        (opt-in explícito para activar)
 * Sin sink/IPC/gate: falla cerrado y no activa. */
#ifdef X6200_LINUX_DSP_ROUTE
typedef enum { ROUTE_QA_DISABLED = 0, ROUTE_QA_READY, ROUTE_QA_ACTIVE, ROUTE_QA_BLOCKED } route_qa_state_t;
int  x6200_route_qa_start_default(void);       /* hooks: sink PulseAudio + env */
int  x6200_route_qa_start(const route_sink_t *sink);  /* tests/GUI: sink inyectado */
int  x6200_route_qa_attach_ipc(int fd);        /* tests: fd ya conectado */
void x6200_route_qa_submit_pcm(const int16_t *samples, size_t count, bool processed);
void x6200_route_qa_generated(bool active);
void x6200_route_qa_tx(bool tx);
int  x6200_route_qa_activate_now(void);        /* QA explícito (no auto fuera de env) */
route_qa_state_t x6200_route_qa_state(void);
int  x6200_route_qa_last_error(void);
void x6200_route_qa_stop(void);
#endif

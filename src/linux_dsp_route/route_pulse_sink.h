#pragma once
#include "route_sink.h"
#ifdef X6200_LINUX_DSP_ROUTE
/* Sink real PulseAudio (simple API) para la ruta QA post-Speex. open/write/close
 * los llama el worker dedicado, nunca el callback de captura. */
route_sink_t route_pulse_sink_create(void);
void route_pulse_sink_destroy(route_sink_t *sink);
#endif

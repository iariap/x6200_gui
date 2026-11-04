#pragma once
#include "route_proxy.h"
/* Interposición de la API pública de libaether para feature ON.
 * Si `proxy` es NULL/inactivo => se delega a la libaether original (BASE).
 * Si está activo => las escrituras del registro 12 van SOLO por el proxy/IPC
 * (sin escritor paralelo); idle no reescribe directo. */
void route_libaether_proxy_set(route_proxy_t *proxy);
route_proxy_t *route_libaether_proxy_get(void);
/* Último error del camino proxy (para diagnóstico/QA; la API original es void). */
route_err_t route_libaether_proxy_last_error(void);

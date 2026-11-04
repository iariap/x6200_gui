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

/* Estado explícito del refresh idle (evita pérdida silenciosa): el último snapshot
 * pendiente se coalesce; los fallos se cuentan y se exponen; no hay pending indefinido. */
typedef struct {
    uint64_t attempts;
    uint64_t failures;
    uint64_t blocked;       /* fallos desde el último refresh exitoso */
    uint64_t coalesced;     /* snapshots reemplazados antes de un intento */
    route_err_t last_error;
    bool pending;           /* snapshot esperando próximo intento */
} route_lib_proxy_refresh_status_t;
void route_libaether_proxy_refresh_status(route_lib_proxy_refresh_status_t *out);

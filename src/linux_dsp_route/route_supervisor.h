#pragma once
#include "route_common.h"

/* Supervisor SEPARADO de la GUI: vigila heartbeat/lease y estado del arbitro.
 * NO escribe el registro 12 (eso es del arbitro). Si el arbitro no responde no
 * puede restaurar de forma independiente => FAULT y bloqueo de activacion. */
typedef enum {
    ROUTE_SUP_NONE = 0,
    ROUTE_SUP_REQUEST_RESTORE,  /* pedir al arbitro restore BASE (budget 250 ms) */
    ROUTE_SUP_FAULT,            /* arbitro no responde/no puede restaurar: bloquear */
} route_sup_action_t;

typedef struct {
    uint64_t last_heartbeat_ms;
    bool gui_alive;
    bool arbiter_responsive;
    uint32_t arb_state;
    bool restore_pending;
    uint64_t restore_deadline_ms;
    uint64_t lease_ms;
    uint64_t budget_ms;
    uint64_t last_restore_latency_ms;
    bool fault;
    bool blocked_physical_recovery;  /* árbitro failed/unavailable: no restore físico seguro */
    uint32_t arbiter_failures;
} route_supervisor_t;

void route_supervisor_init(route_supervisor_t *s, uint64_t lease_ms, uint64_t budget_ms);
void route_supervisor_gui_heartbeat(route_supervisor_t *s, uint64_t now_ms);
void route_supervisor_gui_gone(route_supervisor_t *s);
void route_supervisor_arbiter_status(route_supervisor_t *s, uint64_t now_ms, bool responsive, uint32_t state);
/* Devuelve la acción; no ejecuta I2C. */
route_sup_action_t route_supervisor_tick(route_supervisor_t *s, uint64_t now_ms);
void route_supervisor_note_restored(route_supervisor_t *s, uint64_t now_ms, uint32_t state);
/* El árbitro dejó de ser confiable/terminó: NO hay restore físico seguro desde el
 * supervisor (no escribe reg12); bloquea y marca recuperación física BLOCKED. */
void route_supervisor_arbiter_lost(route_supervisor_t *s);
bool route_supervisor_physical_recovery_blocked(const route_supervisor_t *s);

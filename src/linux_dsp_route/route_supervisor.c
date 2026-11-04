#include "route_supervisor.h"
#include <string.h>

void route_supervisor_init(route_supervisor_t *s, uint64_t lease_ms, uint64_t budget_ms) {
    if (s == NULL) return;
    memset(s, 0, sizeof(*s));
    s->lease_ms = lease_ms ? lease_ms : ROUTE_LEASE_MS;
    s->budget_ms = budget_ms ? budget_ms : ROUTE_RESTORE_BUDGET_MS;
    s->arbiter_responsive = true;
}
void route_supervisor_gui_heartbeat(route_supervisor_t *s, uint64_t now_ms) {
    if (s == NULL) return;
    s->gui_alive = true;
    s->last_heartbeat_ms = now_ms;
}
void route_supervisor_gui_gone(route_supervisor_t *s) { if (s) s->gui_alive = false; }
void route_supervisor_arbiter_lost(route_supervisor_t *s) {
    if (s == NULL) return;
    s->arbiter_responsive = false; s->fault = true; s->blocked_physical_recovery = true;
    s->arbiter_failures++; s->arb_state = ROUTE_STATE_FAULT;
    s->restore_pending = false;
}
bool route_supervisor_physical_recovery_blocked(const route_supervisor_t *s) {
    return s != NULL && s->blocked_physical_recovery;
}
void route_supervisor_arbiter_status(route_supervisor_t *s, uint64_t now_ms, bool responsive, uint32_t state) {
    (void)now_ms;
    if (s == NULL) return;
    s->arbiter_responsive = responsive;
    s->arb_state = state;
    if (!responsive) { s->fault = true; s->blocked_physical_recovery = true; s->arbiter_failures++; }
}
void route_supervisor_note_restored(route_supervisor_t *s, uint64_t now_ms, uint32_t state) {
    if (s == NULL) return;
    /* evidencia de restore vino del árbitro: vuelve a considerarlo responsivo (lógico) */
    s->arbiter_responsive = true;
    s->restore_pending = false;
    s->arb_state = state;
    if (s->last_heartbeat_ms != 0 && now_ms >= s->last_heartbeat_ms)
        s->last_restore_latency_ms = now_ms - s->last_heartbeat_ms;
    if (state == ROUTE_STATE_BASE || state == ROUTE_STATE_DEGRADED_BASE) s->fault = false;
    /* restored = recuperación LÓGICA (requiere evidencia del árbitro); física sigue no confirmada */
    s->blocked_physical_recovery = false;
}
route_sup_action_t route_supervisor_tick(route_supervisor_t *s, uint64_t now_ms) {
    if (s == NULL) return ROUTE_SUP_FAULT;
    if (s->fault) return ROUTE_SUP_FAULT;
    if (!s->arbiter_responsive) return ROUTE_SUP_FAULT;
    if (s->restore_pending) {
        if (now_ms >= s->restore_deadline_ms) return ROUTE_SUP_FAULT;  /* no llegó evidencia de restore */
        return ROUTE_SUP_REQUEST_RESTORE;
    }
    bool active = (s->arb_state == ROUTE_STATE_RX_LINUX || s->arb_state == ROUTE_STATE_READY);
    if (!active) return ROUTE_SUP_NONE;
    if (!s->arbiter_responsive) { s->fault = true; return ROUTE_SUP_FAULT; }
    bool lease_expired = !s->gui_alive ||
        (s->last_heartbeat_ms != 0 && now_ms >= s->last_heartbeat_ms &&
         (now_ms - s->last_heartbeat_ms) > s->lease_ms) ||
        s->last_heartbeat_ms == 0;
    if (lease_expired) {
        s->restore_pending = true;
        s->restore_deadline_ms = now_ms + s->budget_ms;
        return ROUTE_SUP_REQUEST_RESTORE;
    }
    return ROUTE_SUP_NONE;
}

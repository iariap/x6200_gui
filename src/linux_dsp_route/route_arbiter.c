#include "route_arbiter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct route_arbiter {
    route_bus_t bus;
    route_shadow_t shadow;
    route_state_t state;
    bool initialized, enabled, gui_alive, gui_write_ok, generated, sink_ok, capture_ok;
    bool pcm_ok;
    uint32_t hb_value;
    uint64_t hb_ms;
    uint32_t hb_seq, last_hb_seq;
    uint32_t ring_count;
    uint64_t last_block_id;
    route_stats_t stats;
    route_err_t last_err;
};

static int bus_write(route_arbiter_t *a, uint32_t value) {
    if (a->bus.write_reg12 == NULL) { a->stats.bus_errors++; return -1; }
    a->stats.writes++;
    int r = a->bus.write_reg12(a->bus.ctx, value);
    if (r != 0) { a->stats.bus_errors++; route_shadow_invalidate(&a->shadow); }
    else route_shadow_apply_success(&a->shadow, value);
    return r;
}

route_arbiter_t *route_arbiter_create(const route_bus_t *bus, uint64_t now_ms) {
    (void)now_ms;
    route_arbiter_t *a = calloc(1, sizeof(*a));
    if (a == NULL) return NULL;
    if (bus != NULL) a->bus = *bus;
    a->state = ROUTE_STATE_BASE;
    a->enabled = false; a->gui_alive = false; a->gui_write_ok = false;
    a->generated = false; a->sink_ok = true; a->capture_ok = true; a->pcm_ok = false;
    route_shadow_init(&a->shadow, 0);
    a->last_err = ROUTE_ERR_OK;
    return a;
}
void route_arbiter_destroy(route_arbiter_t *a) { free(a); }

route_err_t route_arbiter_boot(route_arbiter_t *a) {
    if (a == NULL) return ROUTE_ERR_INVALID;
    a->enabled = false; a->state = ROUTE_STATE_BASE;
    route_shadow_invalidate(&a->shadow);
    if (bus_write(a, 0u) != 0) {
        a->state = ROUTE_STATE_FAULT; a->stats.fault_count++;
        a->last_err = ROUTE_ERR_BUS; return ROUTE_ERR_BUS;
    }
    a->initialized = true;
    return ROUTE_ERR_OK;
}

route_err_t route_arbiter_client_heartbeat(route_arbiter_t *a, uint64_t now_ms, uint32_t seq,
                                           uint32_t value, bool write_ok) {
    if (a == NULL) return ROUTE_ERR_INVALID;
    if (seq <= a->last_hb_seq) { a->last_err = ROUTE_ERR_STALE; return ROUTE_ERR_STALE; }
    a->last_hb_seq = seq;
    a->gui_alive = true; a->gui_write_ok = write_ok;
    a->hb_ms = now_ms; a->hb_value = value;
    a->stats.last_heartbeat_ms = now_ms;
    return ROUTE_ERR_OK;
}
void route_arbiter_client_gone(route_arbiter_t *a) { if (a) a->gui_alive = false; }

static bool hb_fresh(const route_arbiter_t *a, uint64_t now) {
    return a->gui_alive && a->hb_ms != 0 && now >= a->hb_ms && (now - a->hb_ms) <= ROUTE_LEASE_MS;
}

static route_err_t disable_rx(route_arbiter_t *a, uint64_t now_ms, const char *why) {
    (void)why;
    if (a->state != ROUTE_STATE_RX_LINUX && a->state != ROUTE_STATE_READY) return ROUTE_ERR_OK;
    if (!a->shadow.valid) { a->state = ROUTE_STATE_FAULT; a->stats.fault_count++; a->last_err = ROUTE_ERR_UNKNOWN_STATE; return ROUTE_ERR_UNKNOWN_STATE; }
    uint32_t next = a->shadow.value & ~ROUTE_REG_VOICE_REC;
    if (bus_write(a, next) != 0) { a->state = ROUTE_STATE_FAULT; a->stats.fault_count++; a->last_err = ROUTE_ERR_BUS; return ROUTE_ERR_BUS; }
    a->stats.restores++;
    a->stats.last_restore_ms = now_ms;
    if (a->hb_ms != 0 && now_ms >= a->hb_ms) {
        uint64_t lat = now_ms - a->hb_ms;
        a->stats.last_restore_latency_ms = lat;
        if (lat > a->stats.max_restore_latency_ms) a->stats.max_restore_latency_ms = lat;
    }
    a->enabled = false; a->state = ROUTE_STATE_DEGRADED_BASE;
    return ROUTE_ERR_OK;
}

route_err_t route_arbiter_apply_shared(route_arbiter_t *a, uint64_t now_ms, uint32_t target) {
    if (a == NULL || !a->initialized) return ROUTE_ERR_NOT_READY;
    if (!route_shadow_valid(&a->shadow)) { a->last_err = ROUTE_ERR_UNKNOWN_STATE; return ROUTE_ERR_UNKNOWN_STATE; }
    if ((target & ROUTE_REG_VOICE_REC) != 0) { a->stats.denied++; a->last_err = ROUTE_ERR_OWNERSHIP; return ROUTE_ERR_OWNERSHIP; }
    if (target & ROUTE_TX_ACTIVITY_MASK) {
        route_err_t e = disable_rx(a, now_ms, "tx");
        if (e != ROUTE_ERR_OK) return e;
        a->stats.tx_disables++;
    }
    uint32_t next = target;
    if (a->state == ROUTE_STATE_RX_LINUX) next |= ROUTE_REG_VOICE_REC; /* sólo si no es TX */
    if (bus_write(a, next) != 0) { a->state = ROUTE_STATE_FAULT; a->stats.fault_count++; a->last_err = ROUTE_ERR_BUS; return ROUTE_ERR_BUS; }
    return ROUTE_ERR_OK;
}

route_err_t route_arbiter_set_enabled(route_arbiter_t *a, uint64_t now_ms, bool enabled) {
    if (a == NULL || !a->initialized) { return ROUTE_ERR_NOT_READY; }
    if (!enabled) {
        route_err_t e = disable_rx(a, now_ms, "disable");
        if (e == ROUTE_ERR_OK) { a->enabled = false; a->state = ROUTE_STATE_BASE; }
        return e;
    }
    if (a->state == ROUTE_STATE_FAULT) { a->stats.denied++; a->last_err = ROUTE_ERR_UNKNOWN_STATE; return ROUTE_ERR_UNKNOWN_STATE; }
    if (!route_shadow_valid(&a->shadow)) { a->stats.denied++; a->last_err = ROUTE_ERR_UNKNOWN_STATE; return ROUTE_ERR_UNKNOWN_STATE; }
    if (!hb_fresh(a, now_ms)) { a->stats.denied++; a->last_err = ROUTE_ERR_NOT_READY; return ROUTE_ERR_NOT_READY; }
    if (!a->gui_write_ok) { a->stats.denied++; a->last_err = ROUTE_ERR_NOT_READY; return ROUTE_ERR_NOT_READY; }
    if (!a->pcm_ok) { a->stats.denied++; a->last_err = ROUTE_ERR_NOT_READY; return ROUTE_ERR_NOT_READY; }
    if (a->generated || !a->sink_ok || !a->capture_ok) { a->stats.denied++; a->last_err = ROUTE_ERR_NOT_READY; return ROUTE_ERR_NOT_READY; }
    if ((a->hb_value & ROUTE_TX_ACTIVITY_MASK) != 0 || (a->shadow.value & ROUTE_TX_ACTIVITY_MASK) != 0) {
        a->stats.denied++; a->last_err = ROUTE_ERR_TX_ACTIVE; return ROUTE_ERR_TX_ACTIVE;
    }
    /* Adopta el último valor completo confirmado por la GUI y cambia SÓLO 0x8. */
    uint32_t next = a->hb_value | ROUTE_REG_VOICE_REC;
    if (bus_write(a, next) != 0) { a->state = ROUTE_STATE_FAULT; a->stats.fault_count++; a->last_err = ROUTE_ERR_BUS; return ROUTE_ERR_BUS; }
    a->enabled = true;
    a->state = ROUTE_STATE_RX_LINUX;
    return ROUTE_ERR_OK;
}

route_err_t route_arbiter_tx_prepare(route_arbiter_t *a, uint64_t now_ms) {
    if (a == NULL) return ROUTE_ERR_INVALID;
    route_err_t e = disable_rx(a, now_ms, "tx_prepare");
    if (e != ROUTE_ERR_OK) return e;
    if (a->state == ROUTE_STATE_RX_LINUX || a->state == ROUTE_STATE_READY) a->stats.tx_disables++;
    return a->state == ROUTE_STATE_FAULT ? ROUTE_ERR_BUS : ROUTE_ERR_OK;
}
route_err_t route_arbiter_atu_prepare(route_arbiter_t *a, uint64_t now_ms) { return route_arbiter_tx_prepare(a, now_ms); }

void route_arbiter_set_generated_audio(route_arbiter_t *a, bool active) {
    if (a == NULL) return;
    a->generated = active;
    if (active && (a->state == ROUTE_STATE_RX_LINUX || a->state == ROUTE_STATE_READY)) disable_rx(a, a->hb_ms, "generated");
}
void route_arbiter_set_sink_ok(route_arbiter_t *a, bool ok) {
    if (a == NULL) return;
    a->sink_ok = ok;
    if (!ok && (a->state == ROUTE_STATE_RX_LINUX || a->state == ROUTE_STATE_READY)) disable_rx(a, a->hb_ms, "sink");
}
void route_arbiter_set_capture_ok(route_arbiter_t *a, bool ok) {
    if (a == NULL) return;
    a->capture_ok = ok;
    if (!ok && (a->state == ROUTE_STATE_RX_LINUX || a->state == ROUTE_STATE_READY)) disable_rx(a, a->hb_ms, "capture");
}

route_err_t route_arbiter_pcm_submit(route_arbiter_t *a, size_t samples, uint32_t rate,
                                     bool post_speex, uint64_t block_id, uint64_t now_ms) {
    (void)now_ms;
    if (a == NULL) return ROUTE_ERR_INVALID;
    if (samples == 0 || samples > ROUTE_BLOCK_SAMPLES || rate != ROUTE_RATE || !post_speex) {
        a->pcm_ok = false; a->stats.denied++; return ROUTE_ERR_INVALID;
    }
    if (block_id != 0 && block_id == a->last_block_id) { a->stats.double_process++; a->pcm_ok = false; return ROUTE_ERR_STALE; }
    if (block_id != 0) a->last_block_id = block_id;
    if (a->ring_count == ROUTE_RING_BLOCKS) { a->stats.overflows++; a->ring_count--; }
    a->ring_count++; a->stats.pcm_blocks++;
    a->pcm_ok = true;
    return ROUTE_ERR_OK;
}
route_err_t route_arbiter_pcm_missing(route_arbiter_t *a, uint64_t now_ms) {
    if (a == NULL) return ROUTE_ERR_INVALID;
    a->pcm_ok = false; a->stats.denied++;
    if (a->state == ROUTE_STATE_RX_LINUX || a->state == ROUTE_STATE_READY) return disable_rx(a, now_ms, "pcm_missing");
    return ROUTE_ERR_OK;
}

route_event_t route_arbiter_tick(route_arbiter_t *a, uint64_t now_ms) {
    if (a == NULL) return ROUTE_EV_FAULT;
    if (a->state == ROUTE_STATE_RX_LINUX || a->state == ROUTE_STATE_READY) {
        if (!a->gui_alive || !hb_fresh(a, now_ms)) {
            route_err_t e = disable_rx(a, now_ms, "lease");
            if (e != ROUTE_ERR_OK || a->state == ROUTE_STATE_FAULT) { a->stats.fault_count++; return ROUTE_EV_FAULT; }
            return ROUTE_EV_RESTORED;
        }
    }
    return ROUTE_EV_OK;
}

route_state_t route_arbiter_state(const route_arbiter_t *a) { return a ? a->state : ROUTE_STATE_FAULT; }
bool route_arbiter_enabled(const route_arbiter_t *a) { return a ? a->enabled : false; }
const route_shadow_t *route_arbiter_shadow(const route_arbiter_t *a) { return a ? &a->shadow : NULL; }
const route_stats_t *route_arbiter_stats(const route_arbiter_t *a) { return a ? &a->stats : NULL; }
const char *route_arbiter_last_error_name(const route_arbiter_t *a) {
    static char buf[64];
    if (a == NULL) return "invalid";
    switch (a->last_err) {
        case ROUTE_ERR_OK: return "OK";
        case ROUTE_ERR_INVALID: return "INVALID";
        case ROUTE_ERR_NOT_READY: return "NOT_READY";
        case ROUTE_ERR_UNKNOWN_STATE: return "UNKNOWN_STATE";
        case ROUTE_ERR_BUS: return "BUS";
        case ROUTE_ERR_TIMEOUT: return "TIMEOUT";
        case ROUTE_ERR_STALE: return "STALE";
        case ROUTE_ERR_TX_ACTIVE: return "TX_ACTIVE";
        case ROUTE_ERR_OWNERSHIP: return "OWNERSHIP";
    }
    snprintf(buf, sizeof(buf), "ERR%d", (int)a->last_err);
    return buf;
}
const char *route_state_name(route_state_t s) {
    switch (s) {
        case ROUTE_STATE_BASE: return "BASE";
        case ROUTE_STATE_READY: return "READY";
        case ROUTE_STATE_RX_LINUX: return "RX_LINUX";
        case ROUTE_STATE_DEGRADED_BASE: return "DEGRADED_BASE";
        case ROUTE_STATE_FAULT: return "FAULT";
    }
    return "?";
}
const char *route_err_name(route_err_t e) {
    switch (e) {
        case ROUTE_ERR_OK: return "OK"; case ROUTE_ERR_INVALID: return "INVALID";
        case ROUTE_ERR_NOT_READY: return "NOT_READY"; case ROUTE_ERR_UNKNOWN_STATE: return "UNKNOWN_STATE";
        case ROUTE_ERR_BUS: return "BUS"; case ROUTE_ERR_TIMEOUT: return "TIMEOUT";
        case ROUTE_ERR_STALE: return "STALE"; case ROUTE_ERR_TX_ACTIVE: return "TX_ACTIVE";
        case ROUTE_ERR_OWNERSHIP: return "OWNERSHIP";
    }
    return "?";
}

#define _POSIX_C_SOURCE 200809L
#include "route_proxy.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct route_proxy {
    route_ipc_client_t *ipc;
    uint32_t cached;
    bool enabled, cache_valid;
};

static uint64_t now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

route_proxy_t *route_proxy_create(route_ipc_client_t *ipc, uint32_t cached_reg12, bool enabled) {
    if (!enabled || ipc == NULL) return NULL;
    route_proxy_t *p = calloc(1, sizeof(*p));
    if (p == NULL) return NULL;
    p->ipc = ipc;
    p->cached = cached_reg12 & ROUTE_PRESERVE_MASK;
    p->cache_valid = true;
    p->enabled = true;
    return p;
}
void route_proxy_destroy(route_proxy_t *p) { free(p); }
bool route_proxy_is_enabled(const route_proxy_t *p) { return p != NULL && p->enabled; }

route_err_t route_proxy_get_reg12(route_proxy_t *p, uint32_t *out) {
    if (p == NULL || out == NULL) return ROUTE_ERR_INVALID;
    if (!p->enabled || !p->cache_valid) return ROUTE_ERR_UNKNOWN_STATE;
    *out = p->cached;
    return ROUTE_ERR_OK;
}

route_err_t route_proxy_write_reg12(route_proxy_t *p, uint32_t target) {
    if (p == NULL || !p->enabled) return ROUTE_ERR_NOT_READY;
    if ((target & ROUTE_REG_VOICE_REC) != 0) return ROUTE_ERR_OWNERSHIP;
    if ((target & ROUTE_TX_ACTIVITY_MASK) != 0) {
        /* TX/ATU: el arbitro hace disable-first; pedir prepare antes del shared. */
        route_ipc_msg_t r;
        if (route_ipc_client_request(p->ipc, ROUTE_IPC_REQ_TX_PREPARE, 0, 0, now_ms(), &r, 100) != 0)
            return ROUTE_ERR_TIMEOUT;
        if (r.err != ROUTE_ERR_OK) { p->cache_valid = false; return (route_err_t)r.err; }
    }
    route_ipc_msg_t r;
    if (route_ipc_client_request(p->ipc, ROUTE_IPC_REQ_APPLY_SHARED, target, 0, now_ms(), &r, 100) != 0)
        return ROUTE_ERR_TIMEOUT;
    if (r.err != ROUTE_ERR_OK) { p->cache_valid = false; return (route_err_t)r.err; }
    p->cached = target & ROUTE_PRESERVE_MASK;
    p->cache_valid = true;
    return ROUTE_ERR_OK;
}
static route_err_t rmw(route_proxy_t *p, uint32_t set, uint32_t clear) {
    if (p == NULL || !p->enabled) return ROUTE_ERR_NOT_READY;
    if (!p->cache_valid) return ROUTE_ERR_UNKNOWN_STATE;
    return route_proxy_write_reg12(p, (p->cached | set) & ~clear);
}
route_err_t route_proxy_ptt_set(route_proxy_t *p, bool on) { return rmw(p, on ? ROUTE_REG_IPTT : 0u, on ? 0u : ROUTE_REG_IPTT); }
route_err_t route_proxy_atu_tune(route_proxy_t *p, bool on) { return rmw(p, on ? ROUTE_REG_ATU_TUNE : 0u, on ? 0u : ROUTE_REG_ATU_TUNE); }
route_err_t route_proxy_atu_set(route_proxy_t *p, bool on) { return rmw(p, on ? ROUTE_REG_ATUE : 0u, on ? 0u : ROUTE_REG_ATUE); }
route_err_t route_proxy_modem_set(route_proxy_t *p, bool on) { return rmw(p, on ? ROUTE_REG_MODEM : 0u, on ? 0u : ROUTE_REG_MODEM); }
route_err_t route_proxy_swrscan_set(route_proxy_t *p, bool on) { return rmw(p, on ? ROUTE_REG_SWRSCAN : 0u, on ? 0u : ROUTE_REG_SWRSCAN); }
route_err_t route_proxy_split_set(route_proxy_t *p, bool on) {
    /* x6200_sple = 0x2 (no está en TX mask); el arbiter no lo toca. */
    if (p == NULL || !p->enabled || !p->cache_valid) return ROUTE_ERR_NOT_READY;
    uint32_t next = on ? (p->cached | 0x2u) : (p->cached & ~0x2u);
    return route_proxy_write_reg12(p, next);
}
route_err_t route_proxy_poweroff(route_proxy_t *p) { return rmw(p, ROUTE_REG_POWER_OFF, 0u); }
route_err_t route_proxy_calibration_set(route_proxy_t *p, bool on) { return rmw(p, on ? 0x8000u : 0u, on ? 0u : 0x8000u); }
route_err_t route_proxy_refresh_block(route_proxy_t *p, const uint32_t regs[ROUTE_SNAPSHOT_REGS]) {
    if (p == NULL || !p->enabled || regs == NULL) return ROUTE_ERR_NOT_READY;
    route_ipc_msg_t r;
    if (route_ipc_client_refresh(p->ipc, regs, now_ms(), &r, 100) != 0) return ROUTE_ERR_TIMEOUT;
    return (route_err_t)r.err;
}
route_err_t route_proxy_record_set(route_proxy_t *p, bool on) {
    (void)on;
    if (p == NULL) return ROUTE_ERR_INVALID;
    return ROUTE_ERR_OWNERSHIP;   /* el bit 0x8 es del arbitro, no de la GUI */
}

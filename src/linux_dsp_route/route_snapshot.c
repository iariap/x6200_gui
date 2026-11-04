#include "route_snapshot.h"
#include <string.h>

bool route_snapshot_capture(route_snapshot_t *s, route_snapshot_getter_t get, void *ctx,
                            uint32_t version, uint64_t seq, uint64_t now_ms, uint64_t lease_ms) {
    if (s == NULL || get == NULL) return false;
    memset(s, 0, sizeof(*s));
    for (unsigned i = 0; i < ROUTE_SNAPSHOT_REGS; i++) s->regs[i] = get(ctx, (int)i);
    s->addr = 0; s->version = version; s->seq = seq; s->captured_ms = now_ms;
    s->lease_ms = lease_ms ? lease_ms : 1000u; s->valid = true;
    return true;
}
bool route_snapshot_wire(const route_snapshot_t *s, uint8_t out[ROUTE_SNAPSHOT_BYTES]) {
    if (s == NULL || out == NULL || !s->valid) return false;
    out[0] = (uint8_t)((s->addr >> 8) & 0xFF); out[1] = (uint8_t)(s->addr & 0xFF);
    for (unsigned i = 0; i < ROUTE_SNAPSHOT_REGS; i++) {
        out[2 + 4*i + 0] = (uint8_t)(s->regs[i] & 0xFF);
        out[2 + 4*i + 1] = (uint8_t)((s->regs[i] >> 8) & 0xFF);
        out[2 + 4*i + 2] = (uint8_t)((s->regs[i] >> 16) & 0xFF);
        out[2 + 4*i + 3] = (uint8_t)((s->regs[i] >> 24) & 0xFF);
    }
    return true;
}
bool route_snapshot_authoritative(const route_snapshot_t *s, route_snapshot_getter_t get,
                                  void *ctx, uint64_t now_ms, uint32_t current_version) {
    if (s == NULL || get == NULL || !s->valid) return false;
    if (current_version != s->version) return false;
    if (now_ms < s->captured_ms || now_ms - s->captured_ms > s->lease_ms) return false;
    /* Coherencia del reg12 (idx 12): si libaether no refleja la escritura proxied,
     * el snapshot NO es autoritativo y el refresh debe bloquearse. */
    if (get(ctx, (int)12) != s->regs[12]) return false;
    return true;
}
uint32_t route_snapshot_reg(const route_snapshot_t *s, int index) {
    if (s == NULL || index < 0 || index >= (int)ROUTE_SNAPSHOT_REGS) return 0;
    return s->regs[index];
}

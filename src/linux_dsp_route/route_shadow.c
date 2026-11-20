#include "route_shadow.h"
#include <string.h>

void route_shadow_init(route_shadow_t *s, uint32_t base_value) {
    if (s == NULL) return;
    memset(s, 0, sizeof(*s));
    s->value = base_value & ROUTE_PRESERVE_MASK;
    s->valid = false;   /* desconocido hasta la primera escritura BASE confirmada */
}
void route_shadow_apply_success(route_shadow_t *s, uint32_t written) {
    if (s == NULL) return;
    s->value = written;
    s->epoch++;
    s->valid = true;
}
void route_shadow_invalidate(route_shadow_t *s) {
    if (s == NULL) return;
    s->valid = false;
    s->epoch++;
    s->invalidations++;
}
bool route_shadow_valid(const route_shadow_t *s) { return s != NULL && s->valid; }
uint32_t route_shadow_value(const route_shadow_t *s) { return s ? s->value : 0; }
bool route_shadow_plan_route(const route_shadow_t *s, bool dac, uint32_t *out) {
    if (s == NULL || out == NULL || !s->valid) return false;
    *out = dac ? (s->value | ROUTE_REG_VOICE_REC) : (s->value & ~ROUTE_REG_VOICE_REC);
    return true;
}
bool route_shadow_plan_shared(const route_shadow_t *s, uint32_t target, uint32_t *out) {
    if (s == NULL || out == NULL || !s->valid) return false;
    if ((target & ROUTE_REG_VOICE_REC) != 0) return false; /* el bit de ruta es del arbitro */
    *out = target;
    return true;
}

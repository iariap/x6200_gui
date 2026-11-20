#pragma once
#include "route_common.h"

/* Shadow autoritativo del registro 12. Solo se aplica mediante apply_write()
 * (escritura coordinada del arbitro). Cualquier error/restart/ownership loss
 * lo invalida: un shadow invalido BLOQUEA RX_LINUX. */
typedef struct {
    uint32_t value;
    uint64_t epoch;
    bool valid;
    uint32_t invalidations;
} route_shadow_t;

void route_shadow_init(route_shadow_t *s, uint32_t base_value);
void route_shadow_apply_success(route_shadow_t *s, uint32_t written);
void route_shadow_invalidate(route_shadow_t *s);
bool route_shadow_valid(const route_shadow_t *s);
uint32_t route_shadow_value(const route_shadow_t *s);
/* plan: devuelve el valor a escribir para entrar/salir de RX_LINUX, cambiando
 * SOLO el bit voice_rec; false si el shadow no es valido. */
bool route_shadow_plan_route(const route_shadow_t *s, bool dac, uint32_t *out);
/* plan para bits de TX/ATU preservando el resto; false si invalido o si el
 * valor pedido pisa voice_rec (no se permite en una sola escritura). */
bool route_shadow_plan_shared(const route_shadow_t *s, uint32_t target, uint32_t *out);

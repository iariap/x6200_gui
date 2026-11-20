#pragma once
#include "route_common.h"
#include <stdbool.h>
#include <stdint.h>
/* Snapshot del bloque de control de libaether (all_cmd): addr 2B BE + 54 u32 LE = 218 B.
 * La captura usa la API pública x6200_control_get() (GETTER inyectable). */
#define ROUTE_SNAPSHOT_REGS 54u
#define ROUTE_SNAPSHOT_BYTES (2u + 4u * ROUTE_SNAPSHOT_REGS)

typedef uint32_t (*route_snapshot_getter_t)(void *ctx, int index);

typedef struct {
    uint32_t regs[ROUTE_SNAPSHOT_REGS];
    uint32_t version;      /* epoch de captura */
    uint64_t seq;          /* monotonía del snapshot */
    uint64_t captured_ms;
    uint64_t lease_ms;     /* validez máxima */
    uint16_t addr;         /* 0x0000 en all_cmd original */
    bool valid;
} route_snapshot_t;

/* Captura 0..53 desde la API del SDK (sin escribir). Devuelve false si getter NULL. */
bool route_snapshot_capture(route_snapshot_t *s, route_snapshot_getter_t get, void *ctx,
                            uint32_t version, uint64_t seq, uint64_t now_ms, uint64_t lease_ms);
/* Serializa el bloque 218 B (addr BE + args LE). */
bool route_snapshot_wire(const route_snapshot_t *s, uint8_t out[ROUTE_SNAPSHOT_BYTES]);
/* Valida frescura/lease/secuencia y que el snapshot siga siendo coherente con el
 * valor actual del registro compartido (getter). Si el proxy actualizó el reg12
 * fuera de libaether, el snapshot NO es autoritativo => false (refresh se bloquea). */
bool route_snapshot_authoritative(const route_snapshot_t *s, route_snapshot_getter_t get,
                                  void *ctx, uint64_t now_ms, uint32_t current_version);
uint32_t route_snapshot_reg(const route_snapshot_t *s, int index);

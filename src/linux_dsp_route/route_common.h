/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  X6200 Linux-DSP safe speaker route — infraestructura INTEGRADA, DEFAULT OFF.
 *  Issue #362/#363/#364/#365. No se compila ni se activa por defecto; ver
 *  CMake option X6200_LINUX_DSP_ROUTE (OFF).
 *
 *  Producto: RX capturado por Linux -> SpeexDSP -> router -> DAC/MUX -> parlante
 *  interno; BASE nativo = fallback. Este modulo implementa el ARBITRO unico de
 *  ruta (dueno de las escrituras del registro 12 via IPC) + supervisor separado.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ROUTE_REG_VOICE_REC  0x00000008u
#define ROUTE_REG_SWRSCAN    0x00000010u
#define ROUTE_REG_ATUE       0x00001000u
#define ROUTE_REG_ATU_TUNE   0x00002000u
#define ROUTE_REG_MODEM      0x00004000u
#define ROUTE_REG_POWER_OFF  0x00010000u
#define ROUTE_REG_IPTT       0x00040000u
#define ROUTE_TX_ACTIVITY_MASK (ROUTE_REG_SWRSCAN|ROUTE_REG_ATUE|ROUTE_REG_ATU_TUNE|ROUTE_REG_MODEM|ROUTE_REG_POWER_OFF|ROUTE_REG_IPTT)
#define ROUTE_PRESERVE_MASK (~ROUTE_REG_VOICE_REC)

#define ROUTE_SNAPSHOT_REGS 54u
#define ROUTE_SNAPSHOT_BYTES (2u + 4u * ROUTE_SNAPSHOT_REGS)
#define ROUTE_HEARTBEAT_MAX_MS  50u
#define ROUTE_LEASE_MS         100u
#define ROUTE_RESTORE_BUDGET_MS 250u
#define ROUTE_BLOCK_SAMPLES    4410u    /* 100 ms @ 44100 */
#define ROUTE_RING_BLOCKS         4u
#define ROUTE_RATE             44100u

typedef enum {
    ROUTE_STATE_BASE = 0,
    ROUTE_STATE_READY,
    ROUTE_STATE_RX_LINUX,
    ROUTE_STATE_DEGRADED_BASE,
    ROUTE_STATE_FAULT,
} route_state_t;

typedef enum {
    ROUTE_EV_OK = 0,
    ROUTE_EV_RESTORED,
    ROUTE_EV_DENIED,
    ROUTE_EV_FAULT,
    ROUTE_EV_BUSY,
} route_event_t;

typedef enum {
    ROUTE_ERR_OK = 0,
    ROUTE_ERR_INVALID = -1,
    ROUTE_ERR_NOT_READY = -2,
    ROUTE_ERR_UNKNOWN_STATE = -3,
    ROUTE_ERR_BUS = -4,
    ROUTE_ERR_TIMEOUT = -5,
    ROUTE_ERR_STALE = -6,
    ROUTE_ERR_TX_ACTIVE = -7,
    ROUTE_ERR_OWNERSHIP = -8,
    ROUTE_ERR_UNSUPPORTED = -9,
} route_err_t;

/* Backend de bus: SOLO el arbitro escribe. write_reg12 devuelve 0/errno.
 * No hay readback obligatorio: la verdad es el shadow del arbitro. */
typedef struct {
    int (*write_reg12)(void *ctx, uint32_t value);
    /* Bulk de 218 B (addr BE + 54 u32 LE). NULL si el transporte no lo soporta:
     * el refresh idle queda deshabilitado (no hay 54 writes individuales). */
    int (*write_block)(void *ctx, const uint8_t *block, size_t len);
    void *ctx;
} route_bus_t;

typedef struct {
    uint32_t writes, restores, denied, bus_errors, overflows, pcm_blocks;
    uint32_t double_process, tx_disables, fault_count, invalidations;
    uint64_t last_restore_ms, last_restore_latency_ms, max_restore_latency_ms;
    uint64_t last_refresh_ms;
    uint32_t refreshes;
    uint64_t last_heartbeat_ms;
} route_stats_t;

const char *route_state_name(route_state_t s);
const char *route_err_name(route_err_t e);

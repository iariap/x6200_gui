/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  A-prime (#367): dueno unico + guardian fail-safe del registro 12. Ver
 *  docs/evidencia/2026-10-29-rx-audio-a-prime/ (ownership.md / guardian-contract.md).
 *  NO activa el MUX: sin readback real verificado (Fase R) todas las escrituras
 *  del bit 0x8 devuelven RXMUX_BLOCKED_NO_READBACK y el guardian queda inactivo.
 */

#define _POSIX_C_SOURCE 200809L

#include "rx_audio_mux_guard.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- Logica pura ----------------------------------------------------------- */

uint32_t rxmux_preserve_mask(uint32_t current, bool dac) {
    if (dac) {
        return current | RXMUX_BIT_VOICE_REC;
    }
    return current & ~RXMUX_BIT_VOICE_REC;
}

bool rxmux_has_voice_rec(uint32_t value) {
    return (value & RXMUX_BIT_VOICE_REC) != 0;
}

bool rxmux_has_tx_activity(uint32_t value) {
    return (value & RXMUX_TX_ACTIVITY_MASK) != 0;
}

bool rxmux_foreign_voice_rec(uint32_t shadow, bool coordinator_owns) {
    return !coordinator_owns && rxmux_has_voice_rec(shadow);
}

rxmux_result_t rxmux_plan(uint32_t real_value, bool dac, uint32_t *next_value) {
    if (next_value == NULL) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (rxmux_has_tx_activity(real_value)) {
        return RXMUX_BLOCKED_TX_ACTIVE;
    }

    uint32_t next = rxmux_preserve_mask(real_value, dac);

    /* Invariante de seguridad: fuera del bit 0x8 no se toca NINGUN bit. */
    if ((next & RXMUX_PRESERVE_MASK) != (real_value & RXMUX_PRESERVE_MASK)) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (rxmux_has_voice_rec(next) != dac) {
        return RXMUX_BLOCKED_INVALID;
    }

    *next_value = next;
    return RXMUX_OK;
}

rxmux_result_t rxmux_coordinator_apply(const rxmux_bus_ops_t *ops, bool dac,
                                       uint32_t *out_verified) {
    if (ops == NULL || ops->read_reg12 == NULL || ops->write_reg12 == NULL) {
        /* Sin lectura real del MCU no se escribe el registro compartido. */
        return RXMUX_BLOCKED_NO_READBACK;
    }

    uint32_t real = 0;
    if (ops->read_reg12(ops->ctx, &real) != 0) {
        return RXMUX_BLOCKED_BUS;
    }

    uint32_t next = 0;
    rxmux_result_t r = rxmux_plan(real, dac, &next);
    if (r != RXMUX_OK) {
        return r;
    }

    if (next == real) {
        if (out_verified != NULL) {
            *out_verified = real;
        }
        return RXMUX_OK;   /* ya estaba en el estado pedido: no se escribe */
    }

    if (ops->write_reg12(ops->ctx, next) != 0) {
        return RXMUX_BLOCKED_BUS;
    }

    uint32_t back = 0;
    if (ops->read_reg12(ops->ctx, &back) != 0) {
        return RXMUX_BLOCKED_BUS;
    }
    if (back != next) {
        return RXMUX_BLOCKED_VERIFY;
    }

    if (out_verified != NULL) {
        *out_verified = back;
    }
    return RXMUX_OK;
}

rxmux_result_t rxmux_restore_apply(const rxmux_bus_ops_t *ops, uint32_t *out_verified) {
    if (ops == NULL || ops->read_reg12 == NULL || ops->write_reg12 == NULL) {
        return RXMUX_BLOCKED_NO_READBACK;
    }

    uint32_t real = 0;
    if (ops->read_reg12(ops->ctx, &real) != 0) {
        return RXMUX_BLOCKED_BUS;
    }
    if (!rxmux_has_voice_rec(real)) {
        if (out_verified != NULL) {
            *out_verified = real;
        }
        return RXMUX_OK;   /* ya esta en BASE: no se escribe nada */
    }

    uint32_t next = real & ~RXMUX_BIT_VOICE_REC;
    if ((next & RXMUX_PRESERVE_MASK) != (real & RXMUX_PRESERVE_MASK)) {
        return RXMUX_BLOCKED_INVALID;
    }

    if (ops->write_reg12(ops->ctx, next) != 0) {
        return RXMUX_BLOCKED_BUS;
    }

    uint32_t back = 0;
    if (ops->read_reg12(ops->ctx, &back) != 0) {
        return RXMUX_BLOCKED_BUS;
    }
    if (back != next) {
        return RXMUX_BLOCKED_VERIFY;
    }

    if (out_verified != NULL) {
        *out_verified = back;
    }
    return RXMUX_OK;
}

/* --- IPC ------------------------------------------------------------------- */

size_t rxmux_ipc_encode(const rxmux_ipc_msg_t *msg, uint8_t *buf, size_t cap) {
    if (msg == NULL || buf == NULL || cap < sizeof(rxmux_ipc_msg_t)) {
        return 0;
    }
    memcpy(buf, msg, sizeof(rxmux_ipc_msg_t));
    return sizeof(rxmux_ipc_msg_t);
}

int rxmux_ipc_decode(const uint8_t *buf, size_t len, rxmux_ipc_msg_t *msg) {
    if (buf == NULL || msg == NULL || len != sizeof(rxmux_ipc_msg_t)) {
        return -1;
    }
    memcpy(msg, buf, sizeof(rxmux_ipc_msg_t));
    if (msg->magic != RXMUX_IPC_MAGIC || msg->version != RXMUX_IPC_VERSION) {
        return -1;
    }
    return 0;
}

rxmux_result_t rxmux_arm_validate(const rxmux_ipc_msg_t *msg,
                                  const uint8_t expected_token[RXMUX_TOKEN_LEN],
                                  uint32_t expected_pid, uint32_t expected_starttime) {
    if (msg == NULL || expected_token == NULL) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (msg->magic != RXMUX_IPC_MAGIC || msg->version != RXMUX_IPC_VERSION) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (msg->type != RXMUX_MSG_ARM) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (memcmp(msg->token, expected_token, RXMUX_TOKEN_LEN) != 0) {
        return RXMUX_BLOCKED_INVALID;
    }
    /* Anti PID reciclado: no alcanza con el PID. */
    if (msg->pid != expected_pid || msg->starttime != expected_starttime) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (msg->pid == 0 || msg->starttime == 0 || msg->mono_ms == 0) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (msg->lease_ms == 0 || msg->lease_ms > RXMUX_LEASE_TIMEOUT_MS) {
        return RXMUX_BLOCKED_INVALID;
    }
    /* Sin readback real confirmado del valor de referencia no se arma el guardian. */
    if ((msg->flags & RXMUX_FLAG_READBACK_CONFIRMED) == 0) {
        return RXMUX_BLOCKED_NO_READBACK;
    }
    return RXMUX_OK;
}

void rxmux_guardian_init(rxmux_guardian_t *g) {
    if (g != NULL) {
        memset(g, 0, sizeof(*g));
    }
}

rxmux_result_t rxmux_guardian_arm(rxmux_guardian_t *g, const rxmux_ipc_msg_t *msg,
                                  uint64_t now_ms) {
    if (g == NULL || msg == NULL) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (msg->magic != RXMUX_IPC_MAGIC || msg->version != RXMUX_IPC_VERSION ||
        msg->type != RXMUX_MSG_ARM) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (msg->pid == 0 || msg->starttime == 0 || msg->mono_ms == 0) {
        return RXMUX_BLOCKED_INVALID;
    }
    if (msg->lease_ms == 0 || msg->lease_ms > RXMUX_LEASE_TIMEOUT_MS) {
        return RXMUX_BLOCKED_INVALID;
    }
    if ((msg->flags & RXMUX_FLAG_READBACK_CONFIRMED) == 0) {
        return RXMUX_BLOCKED_NO_READBACK;
    }

    /* El llamador ya valido token/pid/starttime (rxmux_arm_validate) con la tabla
     * de instancias permitidas: aca solo se registra el lease. */
    memcpy(g->token, msg->token, RXMUX_TOKEN_LEN);
    g->pid = msg->pid;
    g->starttime = msg->starttime;
    g->last_progress_ms = now_ms;
    g->last_seq = msg->seq;
    g->base_reg12 = msg->base_reg12;
    g->readback_confirmed = true;
    g->restore_requested = false;
    g->writer_stopped = false;
    g->armed = true;
    return RXMUX_OK;
}

rxmux_guard_action_t rxmux_guardian_heartbeat(rxmux_guardian_t *g,
                                              const rxmux_ipc_msg_t *msg,
                                              uint64_t now_ms) {
    if (g == NULL || msg == NULL || !g->armed) {
        return RXMUX_GUARD_NONE;
    }
    if (msg->type != RXMUX_MSG_HEARTBEAT ||
        msg->magic != RXMUX_IPC_MAGIC ||
        msg->version != RXMUX_IPC_VERSION) {
        return RXMUX_GUARD_NONE;
    }
    if (memcmp(msg->token, g->token, RXMUX_TOKEN_LEN) != 0 ||
        msg->pid != g->pid || msg->starttime != g->starttime) {
        return RXMUX_GUARD_NONE;   /* heartbeat de otra instancia: ignorar */
    }
    if (msg->seq <= g->last_seq) {
        return RXMUX_GUARD_NONE;   /* stale/repetido: no renueva el lease */
    }
    g->last_seq = msg->seq;
    g->last_progress_ms = now_ms;
    return RXMUX_GUARD_NONE;
}

rxmux_guard_action_t rxmux_guardian_tick(rxmux_guardian_t *g, uint64_t now_ms) {
    if (g == NULL || !g->armed) {
        return RXMUX_GUARD_NONE;
    }
    if (now_ms < g->last_progress_ms) {
        return RXMUX_GUARD_NONE;   /* el reloj no retrocede el lease */
    }
    uint64_t elapsed = now_ms - g->last_progress_ms;
    if (elapsed <= RXMUX_LEASE_TIMEOUT_MS) {
        return RXMUX_GUARD_NONE;
    }

    /* Sin readback real el guardian NO escribe: se identifica y se queda en BASE
     * (nunca asume que el bit quedo despejado). */
    if (!g->readback_confirmed) {
        return RXMUX_GUARD_REFUSE_NO_READBACK;
    }

    if (!g->restore_requested) {
        g->restore_requested = true;
        return RXMUX_GUARD_REQUEST_RESTORE;
    }

    if (!g->writer_stopped && elapsed > RXMUX_LEASE_TIMEOUT_MS + RXMUX_KILL_WAIT_MS / 2u) {
        return RXMUX_GUARD_STOP_WRITER;
    }

    if (g->writer_stopped) {
        return RXMUX_GUARD_RESTORE_NOW;
    }
    return RXMUX_GUARD_STOP_WRITER;
}

void rxmux_guardian_mark_writer_stopped(rxmux_guardian_t *g) {
    if (g != NULL) {
        g->writer_stopped = true;
    }
}

/* --- Helpers --------------------------------------------------------------- */

uint64_t rxmux_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t) ts.tv_sec * 1000u + (uint64_t) (ts.tv_nsec / 1000000L);
}

const char *rxmux_result_name(rxmux_result_t r) {
    switch (r) {
        case RXMUX_OK: return "OK";
        case RXMUX_BLOCKED_NO_READBACK: return "BLOCKED_NO_READBACK";
        case RXMUX_BLOCKED_NOT_READY: return "BLOCKED_NOT_READY";
        case RXMUX_BLOCKED_TX_ACTIVE: return "BLOCKED_TX_ACTIVE";
        case RXMUX_BLOCKED_FOREIGN_OWNER: return "BLOCKED_FOREIGN_OWNER";
        case RXMUX_BLOCKED_BUS: return "BLOCKED_BUS";
        case RXMUX_BLOCKED_VERIFY: return "BLOCKED_VERIFY";
        case RXMUX_BLOCKED_INVALID: return "BLOCKED_INVALID";
        default: return "BLOCKED_UNKNOWN";
    }
}

const char *rxmux_action_name(rxmux_guard_action_t a) {
    switch (a) {
        case RXMUX_GUARD_NONE: return "NONE";
        case RXMUX_GUARD_REQUEST_RESTORE: return "REQUEST_RESTORE";
        case RXMUX_GUARD_STOP_WRITER: return "STOP_WRITER";
        case RXMUX_GUARD_RESTORE_NOW: return "RESTORE_NOW";
        case RXMUX_GUARD_REFUSE_NO_READBACK: return "REFUSE_NO_READBACK";
        default: return "UNKNOWN";
    }
}

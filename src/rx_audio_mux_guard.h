/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  A-prime (#367): dueno unico y guardian fail-safe del bit voice_rec del registro 12
 *  compartido con PTT/ATU/modem.
 *
 *  Este modulo es SOLO infraestructura de seguridad. NO activa el MUX: el coordinador
 *  (radio_audio_mux_request) falla cerrado si no hay una lectura real del registro 12
 *  (R_BLOCKED_NO_HARDWARE_TRUTH, ver docs/evidencia/2026-10-29-rx-audio-a-prime/) y el
 *  guardian arranca INACTIVO. Toda la temporizacion es CLOCK_MONOTONIC en ms.
 *
 *  Contrato de lectura real (Fase R): el unico camino aceptado es una lectura del MCU
 *  documentada por REA con dev/addr/request/response/endianness/index/mask. Mientras no
 *  exista, rxmux_bus_ops.read_reg12 no se registra y toda escritura del bit 0x8 queda
 *  bloqueada.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* --- Registro 12 compartido (x6200_sple_atue_trx) --------------------------- */

#define RXMUX_REG_INDEX          12u
#define RXMUX_REG_I2C_ADDR       0x0030u   /* index * 4: framing de la lib Aether */
#define RXMUX_BIT_VOICE_REC      0x00000008u
#define RXMUX_BIT_SWRSCAN        0x00000010u
#define RXMUX_BIT_ATUE           0x00001000u
#define RXMUX_BIT_ATU_TUNE       0x00002000u
#define RXMUX_BIT_MODEM          0x00004000u
#define RXMUX_BIT_POWER_OFF      0x00010000u
#define RXMUX_BIT_IPTT           0x00040000u

/* Bits que impiden entrar al MUX experimental (TX/ATU/modem/barrido/apagado). */
#define RXMUX_TX_ACTIVITY_MASK \
    (RXMUX_BIT_SWRSCAN | RXMUX_BIT_ATUE | RXMUX_BIT_ATU_TUNE | RXMUX_BIT_MODEM | \
     RXMUX_BIT_POWER_OFF | RXMUX_BIT_IPTT)

/* Bits que el coordinador NUNCA puede cambiar (todos menos 0x8). */
#define RXMUX_PRESERVE_MASK      (~RXMUX_BIT_VOICE_REC)

/* --- Guardian: constantes de tiempo (ms, CLOCK_MONOTONIC) ------------------- */

#define RXMUX_HEARTBEAT_MAX_MS   50u    /* el worker publica progreso cada <=50 ms */
#define RXMUX_LEASE_TIMEOUT_MS   100u   /* >100 ms sin progreso = lease vencido */
#define RXMUX_KILL_WAIT_MS       50u    /* espera de cierre del bus tras detener al escritor */
#define RXMUX_RESTORE_BUDGET_MS  250u   /* plazo total desde el ultimo progreso valido */
#define RXMUX_TOKEN_LEN          16u

#define RXMUX_IPC_MAGIC          0x52584D47u   /* 'RXMG' */
#define RXMUX_IPC_VERSION        1u

/* Flag: el arm trae confirmado por readback real el valor de referencia del reg12. */
#define RXMUX_FLAG_READBACK_CONFIRMED 0x00000001u

typedef enum {
    RXMUX_OK = 0,
    RXMUX_BLOCKED_NO_READBACK = -1,   /* R_BLOCKED_NO_HARDWARE_TRUTH */
    RXMUX_BLOCKED_NOT_READY = -2,
    RXMUX_BLOCKED_TX_ACTIVE = -3,
    RXMUX_BLOCKED_FOREIGN_OWNER = -4,
    RXMUX_BLOCKED_BUS = -5,
    RXMUX_BLOCKED_VERIFY = -6,
    RXMUX_BLOCKED_INVALID = -7,
} rxmux_result_t;

/* --- Backend de bus: SOLO se registra si hay readback real verificado -------- */

typedef struct {
    int (*read_reg12)(void *ctx, uint32_t *out);
    int (*write_reg12)(void *ctx, uint32_t value);
    void *ctx;
} rxmux_bus_ops_t;

/* --- Logica pura (testeable sin hardware) ---------------------------------- */

uint32_t rxmux_preserve_mask(uint32_t current, bool dac);
bool     rxmux_has_voice_rec(uint32_t value);
bool     rxmux_has_tx_activity(uint32_t value);
/* true si la sombra muestra voice_rec sin que el coordinador sea el dueno (audio
 * generado/TTS/mensaje de voz/arranque): no se pisa ese dueno. */
bool     rxmux_foreign_voice_rec(uint32_t shadow, bool coordinator_owns);

rxmux_result_t rxmux_plan(uint32_t real_value, bool dac, uint32_t *next_value);
/* Ruta nominal: readback real -> plan -> write -> readback de verificacion. */
rxmux_result_t rxmux_coordinator_apply(const rxmux_bus_ops_t *ops, bool dac,
                                       uint32_t *out_verified);
/* Restore de emergencia: NO exige que TX este inactivo (debe preservar PTT/ATU
 * tal cual), pero SI exige readback real y verifica que solo cambio el bit 0x8. */
rxmux_result_t rxmux_restore_apply(const rxmux_bus_ops_t *ops, uint32_t *out_verified);

/* --- Protocolo IPC guardian <-> GUI (UNIX socket /run/...) ------------------ */

typedef enum {
    RXMUX_MSG_ARM = 1,
    RXMUX_MSG_ARM_ACK,
    RXMUX_MSG_HEARTBEAT,
    RXMUX_MSG_DISARM,
    RXMUX_MSG_RESTORE_REQ,
    RXMUX_MSG_RESTORE_ACK,
    RXMUX_MSG_STOP_WRITER,
    RXMUX_MSG_STATUS,
} rxmux_msg_type_t;

typedef struct __attribute__((__packed__)) {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint8_t  token[RXMUX_TOKEN_LEN];
    uint32_t pid;
    uint32_t starttime;   /* campo 22 de /proc/<pid>/stat: anti PID reciclado */
    uint64_t mono_ms;
    uint32_t seq;
    uint32_t flags;
    uint32_t base_reg12;  /* valor REAL leido antes de armar (readback) */
    uint32_t lease_ms;
} rxmux_ipc_msg_t;

size_t rxmux_ipc_encode(const rxmux_ipc_msg_t *msg, uint8_t *buf, size_t cap);
int    rxmux_ipc_decode(const uint8_t *buf, size_t len, rxmux_ipc_msg_t *msg);

typedef enum {
    RXMUX_GUARD_NONE = 0,
    RXMUX_GUARD_REQUEST_RESTORE,  /* pedir restore al coordinador por socket */
    RXMUX_GUARD_STOP_WRITER,      /* sin ack: detener el proceso escritor y esperar bus */
    RXMUX_GUARD_RESTORE_NOW,      /* escritor detenido: readback->clear->write->verify */
    RXMUX_GUARD_REFUSE_NO_READBACK, /* sin readback real: NO escribir (fail-closed) */
} rxmux_guard_action_t;

typedef struct {
    bool     armed;
    uint8_t  token[RXMUX_TOKEN_LEN];
    uint32_t pid;
    uint32_t starttime;
    uint64_t last_progress_ms;
    uint32_t last_seq;
    uint32_t base_reg12;
    bool     readback_confirmed;
    bool     restore_requested;
    bool     writer_stopped;
} rxmux_guardian_t;

void rxmux_guardian_init(rxmux_guardian_t *g);
rxmux_result_t rxmux_guardian_arm(rxmux_guardian_t *g, const rxmux_ipc_msg_t *msg,
                                  uint64_t now_ms);
rxmux_guard_action_t rxmux_guardian_heartbeat(rxmux_guardian_t *g,
                                              const rxmux_ipc_msg_t *msg,
                                              uint64_t now_ms);
rxmux_guard_action_t rxmux_guardian_tick(rxmux_guardian_t *g, uint64_t now_ms);
void rxmux_guardian_mark_writer_stopped(rxmux_guardian_t *g);

/* Validacion de un ARM: magic/version/tipo/token/pid/starttime/readback. */
rxmux_result_t rxmux_arm_validate(const rxmux_ipc_msg_t *msg,
                                  const uint8_t expected_token[RXMUX_TOKEN_LEN],
                                  uint32_t expected_pid, uint32_t expected_starttime);

/* --- Helper de tiempo ------------------------------------------------------ */

uint64_t rxmux_now_ms(void);   /* CLOCK_MONOTONIC, ms */

/* Nombres para logs/evidencia (no cambian el comportamiento). */
const char *rxmux_result_name(rxmux_result_t r);
const char *rxmux_action_name(rxmux_guard_action_t a);

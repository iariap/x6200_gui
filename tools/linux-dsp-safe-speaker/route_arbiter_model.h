/*
 * QA-LINUX-DSP-ROUTE (#362/#363/#364/#365) — modelo de referencia OFFLINE.
 *
 * NO es código de producción, NO toca I2C/ALSA/radio, NO se integra en src/.
 * Modela el contrato de seguridad de un ARBITRO de ruta (proceso independiente
 * de la GUI) que elimina la dependencia del readback del MCU usando una
 * "verdad de arbitro": el unico escritor conoce el ultimo valor aplicado.
 *
 * Estados: BASE inicial -> READY -> RX_LINUX -> DEGRADED_BASE -> FAULT.
 * Reglas duras: BASE inicial, sin auto-enable, solo un escritor, TX/ATU
 * deshabilitan RX_LINUX antes de tocar bits compartidos, heartbeat/lease,
 * restore independiente <=250 ms (modelo), estado desconocido jamas habilita.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Registro 12 compartido (x6200_sple_atue_trx) */
#define RA_REG_VOICE_REC   0x00000008u
#define RA_REG_SWRSCAN     0x00000010u
#define RA_REG_ATUE        0x00001000u
#define RA_REG_ATU_TUNE    0x00002000u
#define RA_REG_MODEM       0x00004000u
#define RA_REG_POWER_OFF   0x00010000u
#define RA_REG_IPTT        0x00040000u
#define RA_REG_TX_MASK (RA_REG_SWRSCAN|RA_REG_ATUE|RA_REG_ATU_TUNE|RA_REG_MODEM|RA_REG_POWER_OFF|RA_REG_IPTT)
#define RA_PRESERVE_MASK (~RA_REG_VOICE_REC)

#define RA_HEARTBEAT_MS      50u
#define RA_LEASE_MS         100u
#define RA_KILL_WAIT_MS      50u
#define RA_RESTORE_BUDGET_MS 250u
#define RA_BLOCK_SAMPLES    4410u   /* 100 ms @ 44100 */
#define RA_RING_BLOCKS        4u
#define RA_PCM_RATE        44100u

typedef enum {
    RA_BASE = 0,
    RA_READY,
    RA_RX_LINUX,
    RA_DEGRADED_BASE,
    RA_FAULT,
} ra_state_t;

typedef enum {
    RA_TRUTH_UNKNOWN = 0,
    RA_TRUTH_ARBITER_SHADOW,   /* el arbitro es el unico escritor y conoce el valor aplicado */
} ra_truth_t;

typedef enum {
    RA_EV_OK = 0,
    RA_EV_RESTORED,
    RA_EV_DENIED_UNKNOWN,
    RA_EV_DENIED_NOT_READY,
    RA_EV_DENIED_TX,
    RA_EV_DENIED_PCM,
    RA_EV_BUS_FAIL,
    RA_EV_DOUBLE_PROCESS,
    RA_EV_FAULT,
} ra_event_t;

/* Backend simulado: una unica funcion de escritura. El modelo lo llama SOLO
 * desde el rol arbitro (nunca desde heartbeats ni desde la GUI). */
typedef struct {
    int (*write_reg12)(void *ctx, uint32_t value);  /* 0 = OK; otro = error de bus */
    void *ctx;
} ra_backend_t;

typedef struct {
    uint32_t writes, restores, denied, bus_errors, overflows, dropped_blocks, pcm_blocks;
    uint32_t underruns, double_process, tx_disables, fault_count;
    uint64_t last_restore_ms, max_restore_ms, last_restore_latency_ms;
} ra_stats_t;

typedef struct ra ra_t;

ra_t *ra_create(const ra_backend_t *be, uint64_t now_ms);
void ra_destroy(ra_t *ra);
/* Arranque: base inicial conocida SOLO tras una escritura BASE exitosa. */
bool ra_boot_init(ra_t *ra);
/* Heartbeat de la GUI/dueno: entrega el ultimo valor que escribio y si el ioctl
 * de su ultima escritura devolvio OK. No escribe nada. */
bool ra_gui_heartbeat(ra_t *ra, uint64_t now_ms, uint32_t last_written_reg12, bool last_write_ok);
/* La GUI avisa que termina/entra a TX/ATU/suspende. */
void ra_gui_exit(ra_t *ra);
bool ra_tx_prepare(ra_t *ra, uint64_t now_ms);   /* true: seguro tocar PTT */
bool ra_atu_prepare(ra_t *ra, uint64_t now_ms);
void ra_set_generated_audio(ra_t *ra, bool active);
/* Perfiles PCM (simulados, sin audio real): solo el dueno procesado puede enviar. */
bool ra_pcm_submit(ra_t *ra, size_t samples, uint32_t rate, bool already_processed, uint64_t block_id, uint64_t now_ms);
bool ra_set_enabled(ra_t *ra, bool enabled, uint64_t now_ms);
/* Tick del arbitro: detecta lease vencido y restaura BASE. Devuelve evento. */
ra_event_t ra_tick(ra_t *ra, uint64_t now_ms);
/* Fallos simulados del stream/sink; degradan y restauran. */
ra_event_t ra_sink_lost(ra_t *ra, uint64_t now_ms);
ra_event_t ra_pcm_stale(ra_t *ra, uint64_t now_ms);
ra_state_t ra_state(const ra_t *ra);
ra_truth_t ra_truth(const ra_t *ra);
uint32_t ra_shadow(const ra_t *ra);
const ra_stats_t *ra_stats(const ra_t *ra);
uint32_t ra_successful_writes(const ra_t *ra);
const char *ra_state_name(ra_state_t s);

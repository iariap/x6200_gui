/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Router RX procesado (SpeexDSP) hacia el DAC del SoC. Issue #364 / Epic #362.
 *
 *  El callback de captura de PulseAudio solo copia PCM a un ring fijo; un worker
 *  separado abre/escribe su propio stream de reproduccion y solicita al backend
 *  de MUX el cambio BASE<->DAC. El backend real del target sigue fail-closed:
 *  sin readback/restore acreditados (#367) no se selecciona DAC.
 *
 *  `submit` nunca bloquea, no reserva memoria, no llama PulseAudio/I2C y no
 *  espera al worker. La seleccion del MUX se hace siempre desde el worker, nunca
 *  desde el callback de captura.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RX_AUDIO_LIVE_BLOCK_SAMPLES 4410u   /* 100 ms a 44100 Hz */
#define RX_AUDIO_LIVE_BLOCKS        4u      /* 400 ms de ring */
#define RX_AUDIO_LIVE_START_BLOCKS  2u      /* prefill antes de pedir DAC */
#define RX_AUDIO_LIVE_STALE_MS      200u    /* PCM mas viejo: no DAC */
#define RX_AUDIO_LIVE_WORKER_WAIT_MS 5u
#define RX_AUDIO_LIVE_RATE          44100u
#define RX_AUDIO_LIVE_TARGET_BUFFER_MS 200u /* objetivo del stream de salida */
#define RX_AUDIO_LIVE_BACKPRESSURE_MS  200u /* sin espacio por mas tiempo: degradar */

typedef enum {
    RX_AUDIO_LIVE_BASE = 0,
    RX_AUDIO_LIVE_PREPARE_DAC,
    RX_AUDIO_LIVE_RX_DAC,
    RX_AUDIO_LIVE_DEGRADED_BASE,
    /* La ultima seleccion DAC pudo haber ocurrido y la restauracion a BASE no se
     * pudo confirmar: no se declara BASE ni se permite una nueva activacion. */
    RX_AUDIO_LIVE_BASE_RESTORE_UNCONFIRMED,
} rx_audio_live_state_t;

typedef enum {
    RX_AUDIO_LIVE_SUSPEND_NONE = 0,
    RX_AUDIO_LIVE_SUSPEND_TX,
    RX_AUDIO_LIVE_SUSPEND_GENERATED,
    RX_AUDIO_LIVE_SUSPEND_CAPTURE,
    RX_AUDIO_LIVE_SUSPEND_STALE,
    RX_AUDIO_LIVE_SUSPEND_SINK,
    RX_AUDIO_LIVE_SUSPEND_MUX,
    RX_AUDIO_LIVE_SUSPEND_BACKPRESSURE,
    RX_AUDIO_LIVE_SUSPEND_RESTORE,
    RX_AUDIO_LIVE_SUSPEND_SHUTDOWN,
} rx_audio_live_suspend_t;

/* Resultado de una peticion de MUX.
 * OK: la transicion se aplico y fue verificada por el backend.
 * NOT_ENTERED: el backend no aplico ninguna transicion (p.ej. denegado por el
 *   gate de seguridad); el estado fisico anterior sigue vigente.
 * UNKNOWN: el backend no puede asegurar el estado fisico; el router debe
 *   intentar volver a BASE y no declarar BASE hasta confirmarlo. */
typedef enum {
    RX_AUDIO_LIVE_MUX_OK = 0,
    RX_AUDIO_LIVE_MUX_NOT_ENTERED = 1,
    RX_AUDIO_LIVE_MUX_UNKNOWN = 2,
} rx_audio_live_mux_result_t;

/* Resultado de una escritura al stream de salida. */
typedef enum {
    RX_AUDIO_LIVE_WRITE_OK = 0,
    RX_AUDIO_LIVE_WRITE_NO_SPACE = 1,
    RX_AUDIO_LIVE_WRITE_ERROR = -1,
} rx_audio_live_write_result_t;

/* Backend inyectable: target PulseAudio + callback MUX fail-closed; tests host
 * usan un fake sin hardware. `mux_request(false)` es siempre una operacion de
 * restauracion y no puede quedar bloqueada por la autorizacion de entrada. */
typedef struct {
    void *ctx;
    bool (*open)(void *ctx);
    void (*close)(void *ctx);
    bool (*ready)(void *ctx);
    int  (*write)(void *ctx, const int16_t *samples, size_t count);
    bool (*mux_ready)(void *ctx);  /* gate de entrada DAC (#367) */
    int  (*mux_request)(void *ctx, bool dac);
} rx_audio_live_backend_t;

typedef struct {
    uint32_t dropped_blocks;
    uint32_t underruns;
    uint32_t writes;
    uint32_t dac_requests;
    uint32_t base_requests;
    uint32_t opens;
    uint32_t closes;
    uint32_t restore_failures;
    uint32_t backpressure;
    rx_audio_live_suspend_t last_suspend;
} rx_audio_live_stats_t;

void rx_audio_live_init(const rx_audio_live_backend_t *backend);
void rx_audio_live_shutdown(void);

/* La activacion es explicita y por defecto esta deshabilitada. Devuelve false
 * si el router esta en BASE_RESTORE_UNCONFIRMED o no esta inicializado. */
bool rx_audio_live_set_enabled(bool enabled);
bool rx_audio_live_enabled(void);

/* Llamado desde el callback de captura con PCM YA procesado. No bloquea. */
void rx_audio_live_submit(const int16_t *samples, size_t count);

/* Prioridades: TX/ATU y audio generado suspenden el router y piden BASE. */
void rx_audio_live_set_tx(bool active);
void rx_audio_live_set_generated_audio(bool active);
void rx_audio_live_notify_capture_error(void);

rx_audio_live_state_t rx_audio_live_get_state(void);
const char *rx_audio_live_state_name(rx_audio_live_state_t state);
const char *rx_audio_live_suspend_name(rx_audio_live_suspend_t why);
void rx_audio_live_get_stats(rx_audio_live_stats_t *out);
size_t rx_audio_live_pending_samples(void);

/* true solo si un backend #367 acredito el gate y no hay una restauracion
 * pendiente/fallida. El backend real actual devuelve false. */
bool rx_audio_live_can_activate(void);
bool rx_audio_live_restore_unconfirmed(void);

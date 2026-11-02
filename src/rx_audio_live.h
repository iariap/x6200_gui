/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Router RX procesado (SpeexDSP) hacia el DAC del SoC, con seleccion de MUX
 *  fail-closed. Issue #364 / Epic #362.
 *
 *  El callback de captura de PulseAudio solo copia PCM a un ring fijo; un worker
 *  separado abre/escribe su propio stream de reproduccion y solicita al backend
 *  de MUX el cambio BASE<->DAC. En esta entrega el backend real del target
 *  devuelve BLOCKED (no hay readback fisico del registro 12), por lo que el
 *  router queda inerte aunque se habilite. No toca play_stm, TTS/mic/ATU/PTT.
 *
 *  Estados:
 *    BASE -> (enabled, stream READY, 2 bloques frescos) -> PREPARE_DAC
 *         -> (mux_request(true) OK) -> RX_DAC
 *    cualquier fallo/TX/audio generado -> DEGRADED_BASE -> BASE
 *
 *  `submit` nunca bloquea, no reserva memoria ni llama PulseAudio/I2C.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RX_AUDIO_LIVE_BLOCK_SAMPLES 4410u   /* 100 ms a 44100 Hz */
#define RX_AUDIO_LIVE_BLOCKS        4u      /* 400 ms maximo */
#define RX_AUDIO_LIVE_START_BLOCKS  2u      /* dos bloques antes de DAC */
#define RX_AUDIO_LIVE_STALE_MS      200u    /* datos RX mas viejos: no DAC */
#define RX_AUDIO_LIVE_WORKER_WAIT_MS 5u
#define RX_AUDIO_LIVE_RATE          44100u

typedef enum {
    RX_AUDIO_LIVE_BASE = 0,
    RX_AUDIO_LIVE_PREPARE_DAC,
    RX_AUDIO_LIVE_RX_DAC,
    RX_AUDIO_LIVE_DEGRADED_BASE,
} rx_audio_live_state_t;

typedef enum {
    RX_AUDIO_LIVE_SUSPEND_NONE = 0,
    RX_AUDIO_LIVE_SUSPEND_TX,
    RX_AUDIO_LIVE_SUSPEND_GENERATED,
    RX_AUDIO_LIVE_SUSPEND_CAPTURE,
    RX_AUDIO_LIVE_SUSPEND_STALE,
    RX_AUDIO_LIVE_SUSPEND_SINK,
    RX_AUDIO_LIVE_SUSPEND_MUX,
    RX_AUDIO_LIVE_SUSPEND_SHUTDOWN,
} rx_audio_live_suspend_t;

/* Backend inyectable: el target usa PulseAudio + un callback MUX fail-closed;
 * los tests host usan un fake sin hardware. */
typedef struct {
    void *ctx;
    bool (*open)(void *ctx);       /* abrir/conectar stream de RX (no bloquea la captura) */
    void (*close)(void *ctx);      /* cerrar/liberar el stream */
    bool (*ready)(void *ctx);      /* stream de reproduccion READY */
    bool (*write)(void *ctx, const int16_t *samples, size_t count); /* PCM S16NE mono */
    bool (*mux_ready)(void *ctx);  /* false = gate A/B/C no acreditados: no DAC */
    int  (*mux_request)(void *ctx, bool dac); /* 0 = OK; !=0 = fail-closed */
} rx_audio_live_backend_t;

typedef struct {
    uint32_t dropped_blocks;
    uint32_t underruns;
    uint32_t writes;
    uint32_t dac_requests;
    uint32_t base_requests;
    uint32_t opens;
    uint32_t closes;
    rx_audio_live_suspend_t last_suspend;
} rx_audio_live_stats_t;

void rx_audio_live_init(const rx_audio_live_backend_t *backend);
void rx_audio_live_shutdown(void);
void rx_audio_live_set_enabled(bool enabled);
bool rx_audio_live_enabled(void);

/* Llamado desde el callback de captura (hilo PulseAudio) con PCM YA procesado. */
void rx_audio_live_submit(const int16_t *samples, size_t count);

/* Prioridades: TX y audio generado suspenden el router y piden BASE. */
void rx_audio_live_set_tx(bool active);
void rx_audio_live_set_generated_audio(bool active);
void rx_audio_live_notify_capture_error(void);

rx_audio_live_state_t rx_audio_live_get_state(void);
const char *rx_audio_live_state_name(rx_audio_live_state_t state);
const char *rx_audio_live_suspend_name(rx_audio_live_suspend_t why);
void rx_audio_live_get_stats(rx_audio_live_stats_t *out);
size_t rx_audio_live_pending_samples(void);

/* Diagnostico de una eventual activacion fisica: el target actual NO la usa. */
bool rx_audio_live_can_activate(void);

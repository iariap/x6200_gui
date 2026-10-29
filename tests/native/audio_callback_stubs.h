/*
 * Observables del stage del callback de captura (issue #351, parte 2).
 *
 * El stage compila el src/audio.c REAL por inclusion (tests/native/test_audio_callback.c
 * hace `#include "audio.c"`) para poder llamar al static read_callback(), y este modulo
 * da los stubs de PulseAudio/ALSA/consumidores. Nada de lo que se verifica es estatico:
 * el test mira el log de eventos (quien recibio que y en que orden), los contadores de
 * drop/peek/write, y el contenido de la memoria que simula el fragmento de PulseAudio
 * contra un canario.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Tipos de evento del log, en el orden en que el callback los produce. */
enum {
    AUDIO_EV_PEEK = 0,
    AUDIO_EV_PROC,
    AUDIO_EV_SCREEN,
    AUDIO_EV_DSP,
    AUDIO_EV_NR_RESET,
    AUDIO_EV_DROP,
    AUDIO_EV_WRITE,
    AUDIO_EV_CTRL,
    AUDIO_EV_LOCK
};

/* Niveles de log que el stage cuenta (los macros LV_LOG_* del stub los usan). */
#define AUDIO_STUB_LOG_USER  0
#define AUDIO_STUB_LOG_WARN  1
#define AUDIO_STUB_LOG_ERROR 2

void audio_stub_log(int level, const char *fmt, ...);

#define AUDIO_STUB_MAX_EVENTS  1024
#define AUDIO_STUB_MAX_SAMPLES 131072

typedef struct {
    int    kind;
    size_t samples;
} audio_stub_event_t;

extern audio_stub_event_t audio_stub_events[AUDIO_STUB_MAX_EVENTS];
extern int                audio_stub_event_count;
extern bool               audio_stub_event_overflow;

/* Respuesta del proximo pa_stream_peek(): rc, puntero y nbytes tal cual. */
void audio_stub_set_peek(int rc, const void *data, size_t nbytes);

/* Retorno del proximo pa_stream_drop() (< 0 simula fallo). */
void audio_stub_set_drop_rc(int rc);

/* Limpia contadores, log y las copias de los consumidores. NO toca el estado del NR ni
 * la configuracion de peek/drop (para encadenar casos sobre el mismo estado). */
void audio_stub_reset_log(void);

/* Contadores acumulados. */
extern int    audio_stub_peek_calls;
extern int    audio_stub_drop_calls;
extern int    audio_stub_process_calls;
extern int    audio_stub_nr_reset_calls;
extern int    audio_stub_screen_calls;
extern int    audio_stub_dsp_calls;
extern int    audio_stub_write_calls;
extern int    audio_stub_ctrl_calls;
extern int    audio_stub_lock_calls;
extern int    audio_stub_warn_logs;
extern int    audio_stub_error_logs;
extern size_t audio_stub_alloc_calls;

/* Canario del estado del NR: cualquier audio_enh_process() lo ensucia; el reset pendiente
 * (el que el hole tiene que disparar) lo limpia. */
extern bool audio_stub_nr_dirty;

/* Lo que recibio cada consumidor, en orden. */
extern int16_t audio_stub_screen_samples[AUDIO_STUB_MAX_SAMPLES];
extern size_t  audio_stub_screen_len;
extern int16_t audio_stub_dsp_samples[AUDIO_STUB_MAX_SAMPLES];
extern size_t  audio_stub_dsp_len;

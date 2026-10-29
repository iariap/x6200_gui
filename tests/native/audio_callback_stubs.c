/*
 * Stubs del stage del callback de captura (issue #351, parte 2).
 *
 * Reemplazan a PulseAudio, ALSA, el SDK de la radio y los dos consumidores del PCM
 * (screen_rec, dsp) por registradores: todo lo que el contrato del callback exige
 * (drop exacto, cero escrituras al fragmento, orden y cantidad de muestras, cero
 * llamadas de reproduccion/ruteo) se verifica mirando estos contadores y este log.
 *
 * Las funciones de malloc/calloc/realloc/free se compilan con -Wl,--wrap=... para poder
 * afirmar que el callback no reserva memoria dinamica (contrato A: nada de heap por
 * fragmento).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alsa/asoundlib.h>
#include <pulse/pulseaudio.h>

#include "params/params.h"

#include "audio_callback_stubs.h"

audio_stub_event_t audio_stub_events[AUDIO_STUB_MAX_EVENTS];
int                audio_stub_event_count;
bool               audio_stub_event_overflow;

int    audio_stub_peek_calls;
int    audio_stub_drop_calls;
int    audio_stub_process_calls;
int    audio_stub_nr_reset_calls;
int    audio_stub_screen_calls;
int    audio_stub_dsp_calls;
int    audio_stub_write_calls;
int    audio_stub_ctrl_calls;
int    audio_stub_lock_calls;
int    audio_stub_warn_logs;
int    audio_stub_error_logs;
size_t audio_stub_alloc_calls;

bool audio_stub_nr_dirty;

int16_t audio_stub_screen_samples[AUDIO_STUB_MAX_SAMPLES];
size_t  audio_stub_screen_len;
int16_t audio_stub_dsp_samples[AUDIO_STUB_MAX_SAMPLES];
size_t  audio_stub_dsp_len;

params_t params = { .hmic = 1, .imic = 1 };

static int    peek_rc;
static const void *peek_data;
static size_t peek_nbytes;
static int    drop_rc;

/* --- heap del callback ------------------------------------------------------ */

void *__real_malloc(size_t size);
void *__real_calloc(size_t nmemb, size_t size);
void *__real_realloc(void *ptr, size_t size);
void  __real_free(void *ptr);

void *__wrap_malloc(size_t size) {
    audio_stub_alloc_calls++;
    return __real_malloc(size);
}

void *__wrap_calloc(size_t nmemb, size_t size) {
    audio_stub_alloc_calls++;
    return __real_calloc(nmemb, size);
}

void *__wrap_realloc(void *ptr, size_t size) {
    audio_stub_alloc_calls++;
    return __real_realloc(ptr, size);
}

void __wrap_free(void *ptr) {
    if (ptr != NULL) {
        audio_stub_alloc_calls++;
    }
    __real_free(ptr);
}

/* --- observabilidad --------------------------------------------------------- */

static void record(int kind, size_t samples) {
    if (audio_stub_event_count >= AUDIO_STUB_MAX_EVENTS) {
        audio_stub_event_overflow = true;
        return;
    }

    audio_stub_events[audio_stub_event_count].kind    = kind;
    audio_stub_events[audio_stub_event_count].samples = samples;
    audio_stub_event_count++;
}

void audio_stub_reset_log(void) {
    audio_stub_event_count    = 0;
    audio_stub_event_overflow = false;
    audio_stub_peek_calls     = 0;
    audio_stub_drop_calls     = 0;
    audio_stub_process_calls  = 0;
    audio_stub_nr_reset_calls = 0;
    audio_stub_screen_calls   = 0;
    audio_stub_dsp_calls      = 0;
    audio_stub_write_calls    = 0;
    audio_stub_ctrl_calls     = 0;
    audio_stub_lock_calls     = 0;
    audio_stub_warn_logs      = 0;
    audio_stub_error_logs     = 0;
    audio_stub_screen_len     = 0;
    audio_stub_dsp_len        = 0;
}

void audio_stub_set_peek(int rc, const void *data, size_t nbytes) {
    peek_rc     = rc;
    peek_data   = data;
    peek_nbytes = nbytes;
}

void audio_stub_set_drop_rc(int rc) {
    drop_rc = rc;
}

void audio_stub_log(int level, const char *fmt, ...) {
    (void) fmt;

    if (level == AUDIO_STUB_LOG_WARN) {
        audio_stub_warn_logs++;
    } else if (level == AUDIO_STUB_LOG_ERROR) {
        audio_stub_error_logs++;
    }
}

/* --- consumidores del PCM --------------------------------------------------- */

static void capture(int16_t *dst, size_t *dst_len, size_t nsamples, int16_t *samples) {
    if (*dst_len + nsamples <= AUDIO_STUB_MAX_SAMPLES) {
        memcpy(dst + *dst_len, samples, nsamples * sizeof(int16_t));
        *dst_len += nsamples;
    }
}

void screen_rec_put_audio_samples(size_t nsamples, int16_t *samples) {
    audio_stub_screen_calls++;
    record(AUDIO_EV_SCREEN, nsamples);
    capture(audio_stub_screen_samples, &audio_stub_screen_len, nsamples, samples);
}

void dsp_put_audio_samples(size_t nsamples, int16_t *samples) {
    audio_stub_dsp_calls++;
    record(AUDIO_EV_DSP, nsamples);
    capture(audio_stub_dsp_samples, &audio_stub_dsp_len, nsamples, samples);
}

void dsp_init(void) {
}

/* --- audio enhancement ------------------------------------------------------ */

void audio_enh_init(void) {
}

/* Speex procesa IN PLACE: el stub tambien, asi el canario del fragmento detecta que el
 * callback viejo (que le pasaba el buffer de PulseAudio) escribe memoria ajena. */
void audio_enh_process(int16_t *samples, size_t nsamples) {
    audio_stub_process_calls++;
    audio_stub_nr_dirty = true;
    record(AUDIO_EV_PROC, nsamples);

    for (size_t i = 0; i < nsamples; i++) {
        samples[i] = (int16_t) (samples[i] / 2);
    }
}

void audio_enh_nr_reset_pending(void) {
    audio_stub_nr_reset_calls++;
    audio_stub_nr_dirty = false;
    record(AUDIO_EV_NR_RESET, 0);
}

/* --- ruteo / control de la radio (el callback no puede llamarlas) ----------- */

void x6200_control_hmic_set(uint8_t value) {
    (void) value;
    audio_stub_ctrl_calls++;
    record(AUDIO_EV_CTRL, 0);
}

void x6200_control_imic_set(uint8_t value) {
    (void) value;
    audio_stub_ctrl_calls++;
    record(AUDIO_EV_CTRL, 0);
}

void x6200_control_record_set(bool on) {
    (void) on;
    audio_stub_ctrl_calls++;
    record(AUDIO_EV_CTRL, 0);
}

/* --- PulseAudio ------------------------------------------------------------- */

int pa_stream_peek(pa_stream *s, const void **data, size_t *nbytes) {
    (void) s;

    audio_stub_peek_calls++;
    record(AUDIO_EV_PEEK, peek_nbytes);

    /* Con el rc de error el stream no garantiza nada, pero el stub entrega igual un
     * puntero valido: asi el callback VIEJO (que no mira el retorno) procesa datos en
     * lugar de caerse, y la falla del test describe el defecto en vez de un SIGSEGV. */
    *data   = peek_data;
    *nbytes = peek_nbytes;
    return peek_rc;
}

int pa_stream_drop(pa_stream *s) {
    (void) s;

    audio_stub_drop_calls++;
    record(AUDIO_EV_DROP, 0);
    return drop_rc;
}

int pa_stream_write(pa_stream *s, const void *data, size_t nbytes, void *free_cb,
                    int64_t offset, pa_seek_mode_t seek) {
    (void) s;
    (void) data;
    (void) nbytes;
    (void) free_cb;
    (void) offset;
    (void) seek;

    audio_stub_write_calls++;
    record(AUDIO_EV_WRITE, 0);
    return 0;
}

void pa_threaded_mainloop_lock(pa_threaded_mainloop *m) {
    (void) m;
    audio_stub_lock_calls++;
    record(AUDIO_EV_LOCK, 0);
}

void pa_threaded_mainloop_unlock(pa_threaded_mainloop *m) {
    (void) m;
    audio_stub_lock_calls++;
    record(AUDIO_EV_LOCK, 0);
}

pa_threaded_mainloop *pa_threaded_mainloop_new(void) {
    return NULL;
}

void pa_threaded_mainloop_start(pa_threaded_mainloop *m) {
    (void) m;
}

void pa_threaded_mainloop_wait(pa_threaded_mainloop *m) {
    (void) m;
}

void pa_threaded_mainloop_signal(pa_threaded_mainloop *m, int wait_for_accept) {
    (void) m;
    (void) wait_for_accept;
}

pa_mainloop_api *pa_threaded_mainloop_get_api(pa_threaded_mainloop *m) {
    (void) m;
    return NULL;
}

pa_context *pa_context_new(pa_mainloop_api *api, const char *name) {
    (void) api;
    (void) name;
    return NULL;
}

void pa_context_set_state_callback(pa_context *c, pa_context_notify_cb_t cb, void *userdata) {
    (void) c;
    (void) cb;
    (void) userdata;
}

int pa_context_connect(pa_context *c, const char *server, void *flags, void *api) {
    (void) c;
    (void) server;
    (void) flags;
    (void) api;
    return -1;
}

pa_context_state_t pa_context_get_state(pa_context *c) {
    (void) c;
    return PA_CONTEXT_FAILED;
}

int pa_context_errno(pa_context *c) {
    (void) c;
    return 0;
}

pa_stream *pa_stream_new(pa_context *c, const char *name, const pa_sample_spec *ss, void *map) {
    (void) c;
    (void) name;
    (void) ss;
    (void) map;
    return NULL;
}

void pa_stream_set_read_callback(pa_stream *s, pa_stream_request_cb_t cb, void *userdata) {
    (void) s;
    (void) cb;
    (void) userdata;
}

int pa_stream_connect_playback(pa_stream *s, const char *dev, const pa_buffer_attr *attr,
                               pa_stream_flags_t flags, void *volume, void *sync) {
    (void) s;
    (void) dev;
    (void) attr;
    (void) flags;
    (void) volume;
    (void) sync;
    return -1;
}

int pa_stream_connect_record(pa_stream *s, const char *dev, const pa_buffer_attr *attr,
                             pa_stream_flags_t flags) {
    (void) s;
    (void) dev;
    (void) attr;
    (void) flags;
    return -1;
}

pa_stream_state_t pa_stream_get_state(pa_stream *s) {
    (void) s;
    return PA_STREAM_FAILED;
}

size_t pa_stream_writable_size(pa_stream *s) {
    (void) s;
    return 0;
}

pa_operation *pa_stream_drain(pa_stream *s, void *cb, void *userdata) {
    (void) s;
    (void) cb;
    (void) userdata;
    return NULL;
}

pa_operation_state_t pa_operation_get_state(pa_operation *o) {
    (void) o;
    return PA_OPERATION_DONE;
}

void pa_operation_unref(pa_operation *o) {
    (void) o;
}

size_t pa_usec_to_bytes(pa_usec_t t, const pa_sample_spec *spec) {
    return (size_t) (t * spec->rate / PA_USEC_PER_MSEC / 1000U) * spec->channels * 2U;
}

uint64_t pa_rtclock_now(void) {
    return 1;
}

const char *pa_strerror(int error) {
    (void) error;
    return "stub";
}

/* --- ALSA (el callback no la toca; solo tiene que linkear) ------------------ */

int snd_mixer_open(snd_mixer_t **mixer, int mode) {
    (void) mode;
    *mixer = NULL;
    return -1;
}

int snd_mixer_attach(snd_mixer_t *mixer, const char *name) {
    (void) mixer;
    (void) name;
    return -1;
}

int snd_mixer_selem_register(snd_mixer_t *mixer, void *options, void **classp) {
    (void) mixer;
    (void) options;
    (void) classp;
    return -1;
}

int snd_mixer_load(snd_mixer_t *mixer) {
    (void) mixer;
    return -1;
}

int snd_mixer_close(snd_mixer_t *mixer) {
    (void) mixer;
    return 0;
}

void snd_mixer_selem_id_set_index(snd_mixer_selem_id_t *id, unsigned int index) {
    if (id != NULL) {
        id->index = (int) index;
    }
}

void snd_mixer_selem_id_set_name(snd_mixer_selem_id_t *id, const char *name) {
    if (id != NULL && name != NULL) {
        snprintf(id->name, sizeof(id->name), "%s", name);
    }
}

snd_mixer_elem_t *snd_mixer_find_selem(snd_mixer_t *mixer, const snd_mixer_selem_id_t *id) {
    (void) mixer;
    (void) id;
    return NULL;
}

int snd_mixer_selem_set_playback_dB_all(snd_mixer_elem_t *elem, long value, int dir) {
    (void) elem;
    (void) value;
    (void) dir;
    return -1;
}

int snd_mixer_selem_set_capture_dB_all(snd_mixer_elem_t *elem, long value, int dir) {
    (void) elem;
    (void) value;
    (void) dir;
    return -1;
}

int snd_mixer_selem_get_playback_dB(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel,
                                    long *value) {
    (void) elem;
    (void) channel;
    (void) value;
    return -1;
}

int snd_mixer_selem_get_capture_dB(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel,
                                   long *value) {
    (void) elem;
    (void) channel;
    (void) value;
    return -1;
}

/*
 * Minimal PulseAudio stub for the capture-callback stage (issue #351, parte 2).
 *
 * Solo declara la superficie que src/audio.c usa. Lo que el test necesita observar
 * -pa_stream_peek(), pa_stream_drop() y las tres API que el callback NO debe tocar
 * (escritura al sink, lock del mainloop)- lo implementa tests/native/audio_callback_stubs.c
 * con contadores y un log de eventos; el resto es un no-op.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint64_t pa_usec_t;

typedef struct pa_threaded_mainloop pa_threaded_mainloop;
typedef struct pa_mainloop_api      pa_mainloop_api;
typedef struct pa_context           pa_context;
typedef struct pa_stream            pa_stream;
typedef struct pa_operation         pa_operation;

typedef enum {
    PA_SAMPLE_U8 = 0,
    PA_SAMPLE_S16LE,
    PA_SAMPLE_S16BE,
    PA_SAMPLE_S16NE = PA_SAMPLE_S16LE,
    PA_SAMPLE_MAX,
    PA_SAMPLE_INVALID = -1
} pa_sample_format_t;

typedef enum {
    PA_STREAM_NODIRECTION = 0,
    PA_STREAM_PLAYBACK,
    PA_STREAM_RECORD
} pa_stream_direction_t;

typedef enum {
    PA_STREAM_UNCONNECTED = 0,
    PA_STREAM_CREATING,
    PA_STREAM_READY,
    PA_STREAM_FAILED,
    PA_STREAM_TERMINATED
} pa_stream_state_t;

typedef enum {
    PA_CONTEXT_UNCONNECTED = 0,
    PA_CONTEXT_CONNECTING,
    PA_CONTEXT_AUTHORIZING,
    PA_CONTEXT_SETTING_NAME,
    PA_CONTEXT_READY,
    PA_CONTEXT_FAILED,
    PA_CONTEXT_TERMINATED
} pa_context_state_t;

#define PA_CONTEXT_IS_GOOD(st) \
    ((st) == PA_CONTEXT_UNCONNECTED || (st) == PA_CONTEXT_CONNECTING || \
     (st) == PA_CONTEXT_AUTHORIZING || (st) == PA_CONTEXT_SETTING_NAME || \
     (st) == PA_CONTEXT_READY)

typedef enum {
    PA_OPERATION_RUNNING = 0,
    PA_OPERATION_DONE,
    PA_OPERATION_CANCELLED
} pa_operation_state_t;

typedef enum {
    PA_SEEK_RELATIVE = 0,
    PA_SEEK_ABSOLUTE,
    PA_SEEK_RELATIVE_ON_READ,
    PA_SEEK_RELATIVE_END
} pa_seek_mode_t;

typedef enum {
    PA_STREAM_NODIRECTION_FLAGS = 0,
    PA_STREAM_ADJUST_LATENCY = 0x2000,
    PA_STREAM_PEAK_DETECT = 0x0002
} pa_stream_flags_t;

typedef struct {
    pa_sample_format_t format;
    uint32_t           rate;
    uint8_t            channels;
} pa_sample_spec;

typedef struct {
    uint32_t maxlength, tlength, prebuf, minreq, fragsize;
} pa_buffer_attr;

#define PA_USEC_PER_MSEC 1000U

typedef void (*pa_stream_request_cb_t)(pa_stream *s, size_t nbytes, void *userdata);
typedef void (*pa_context_notify_cb_t)(pa_context *c, void *userdata);

pa_threaded_mainloop *pa_threaded_mainloop_new(void);
void                  pa_threaded_mainloop_start(pa_threaded_mainloop *m);
void                  pa_threaded_mainloop_lock(pa_threaded_mainloop *m);
void                  pa_threaded_mainloop_unlock(pa_threaded_mainloop *m);
void                  pa_threaded_mainloop_wait(pa_threaded_mainloop *m);
void                  pa_threaded_mainloop_signal(pa_threaded_mainloop *m, int wait_for_accept);
pa_mainloop_api      *pa_threaded_mainloop_get_api(pa_threaded_mainloop *m);

pa_context *pa_context_new(pa_mainloop_api *api, const char *name);
void        pa_context_set_state_callback(pa_context *c, pa_context_notify_cb_t cb, void *userdata);
int         pa_context_connect(pa_context *c, const char *server, void *flags, void *api);
pa_context_state_t pa_context_get_state(pa_context *c);
int         pa_context_errno(pa_context *c);

pa_stream *pa_stream_new(pa_context *c, const char *name, const pa_sample_spec *ss, void *map);
void       pa_stream_set_read_callback(pa_stream *s, pa_stream_request_cb_t cb, void *userdata);
int        pa_stream_connect_playback(pa_stream *s, const char *dev, const pa_buffer_attr *attr,
                                      pa_stream_flags_t flags, void *volume, void *sync);
int        pa_stream_connect_record(pa_stream *s, const char *dev, const pa_buffer_attr *attr,
                                    pa_stream_flags_t flags);
pa_stream_state_t pa_stream_get_state(pa_stream *s);
size_t     pa_stream_writable_size(pa_stream *s);
int        pa_stream_write(pa_stream *s, const void *data, size_t nbytes, void *free_cb,
                           int64_t offset, pa_seek_mode_t seek);
pa_operation *pa_stream_drain(pa_stream *s, void *cb, void *userdata);
int        pa_stream_peek(pa_stream *s, const void **data, size_t *nbytes);
int        pa_stream_drop(pa_stream *s);

pa_operation_state_t pa_operation_get_state(pa_operation *o);
void                 pa_operation_unref(pa_operation *o);

size_t   pa_usec_to_bytes(pa_usec_t t, const pa_sample_spec *spec);
uint64_t pa_rtclock_now(void);
const char *pa_strerror(int error);

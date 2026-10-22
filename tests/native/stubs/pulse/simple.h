/*
 * Minimal PulseAudio simple-API stub for the host unit tests.
 * The wfview server only uses it from its audio thread, which the tests never
 * drive, so returning failure is enough.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct pa_simple pa_simple;
typedef struct pa_channel_map pa_channel_map;

typedef enum {
    PA_STREAM_NODIRECTION,
    PA_STREAM_PLAYBACK,
    PA_STREAM_RECORD,
    PA_STREAM_UPLOAD
} pa_stream_direction_t;

typedef enum {
    PA_SAMPLE_U8,
    PA_SAMPLE_ALAW,
    PA_SAMPLE_ULAW,
    PA_SAMPLE_S16LE,
    PA_SAMPLE_S16BE,
    PA_SAMPLE_FLOAT32LE,
    PA_SAMPLE_FLOAT32BE,
    PA_SAMPLE_S32LE,
    PA_SAMPLE_S32BE,
    PA_SAMPLE_S24LE,
    PA_SAMPLE_S24BE,
    PA_SAMPLE_S24_32LE,
    PA_SAMPLE_S24_32BE,
    PA_SAMPLE_MAX,
    PA_SAMPLE_INVALID = -1
} pa_sample_format_t;

typedef struct {
    pa_sample_format_t format;
    uint32_t           rate;
    uint8_t            channels;
} pa_sample_spec;

typedef struct {
    uint32_t maxlength, tlength, prebuf, minreq, fragsize;
} pa_buffer_attr;

pa_simple *pa_simple_new(const char *server, const char *name, pa_stream_direction_t dir,
                         const char *dev, const char *stream_name, const pa_sample_spec *ss,
                         const pa_channel_map *map, const pa_buffer_attr *attr, int *error);
int        pa_simple_read(pa_simple *s, void *data, size_t bytes, int *error);
void       pa_simple_free(pa_simple *s);

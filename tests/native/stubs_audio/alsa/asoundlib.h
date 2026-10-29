/*
 * Minimal ALSA mixer stub for the capture-callback stage (issue #351, parte 2).
 *
 * src/audio.c compila mixer_setup() y los dos setters de volumen, pero el test nunca
 * entra ahi: solo se ejercita el read_callback. Las funciones quedan declaradas para que
 * el stage linkee y devuelven error, igual que un host sin tarjeta de sonido.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <alloca.h>

typedef struct _snd_mixer snd_mixer_t;
typedef struct _snd_mixer_elem snd_mixer_elem_t;
typedef struct {
    int  dev;
    int  index;
    char name[64];
} snd_mixer_selem_id_t;

typedef enum {
    SND_MIXER_SCHN_FRONT_LEFT = 0,
    SND_MIXER_SCHN_MONO = 2
} snd_mixer_selem_channel_id_t;

#define snd_mixer_selem_id_alloca(ptr) \
    (*(ptr) = (snd_mixer_selem_id_t *) alloca(sizeof(snd_mixer_selem_id_t)))

int  snd_mixer_open(snd_mixer_t **mixer, int mode);
int  snd_mixer_attach(snd_mixer_t *mixer, const char *name);
int  snd_mixer_selem_register(snd_mixer_t *mixer, void *options, void **classp);
int  snd_mixer_load(snd_mixer_t *mixer);
int  snd_mixer_close(snd_mixer_t *mixer);
void snd_mixer_selem_id_set_index(snd_mixer_selem_id_t *id, unsigned int index);
void snd_mixer_selem_id_set_name(snd_mixer_selem_id_t *id, const char *name);
snd_mixer_elem_t *snd_mixer_find_selem(snd_mixer_t *mixer, const snd_mixer_selem_id_t *id);
int  snd_mixer_selem_set_playback_dB_all(snd_mixer_elem_t *elem, long value, int dir);
int  snd_mixer_selem_set_capture_dB_all(snd_mixer_elem_t *elem, long value, int dir);
int  snd_mixer_selem_get_playback_dB(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel,
                                     long *value);
int  snd_mixer_selem_get_capture_dB(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel,
                                    long *value);

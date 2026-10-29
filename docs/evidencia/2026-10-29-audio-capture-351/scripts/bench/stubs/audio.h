/*
 * Stub de audio.h para el banco: src/audio_enhancement.c incluye "audio.h" solo
 * para AUDIO_CAPTURE_RATE (la tasa REAL del stream capturado) y verificar en
 * tiempo de compilacion que la trama de Speex sea 10 ms de esa tasa. El
 * audio.h real arrastra PulseAudio/ALSA, que en el banco no existen.
 *
 * El valor tiene que ser el del firmware (src/audio.h:15): si cambia alla y no
 * aca, el banco mide otra tasa y el #error de audio_enhancement.c lo dice.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#define AUDIO_CAPTURE_RATE  (44100)

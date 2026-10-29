/*
 * Minimal LVGL stub for the capture-callback stage (issue #351, parte 2).
 *
 * src/audio.c solo usa los macros de log. Aca NO se descartan como en los otros stages:
 * el test los cuenta, porque el contrato exige que un error de peek/drop se registre y
 * que un nbytes impar deje una advertencia.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Niveles y registro: los declara el header de observables del stage, que tambien
 * compila el contador (tests/native/audio_callback_stubs.c) para no duplicarlos. */
#include "audio_callback_stubs.h"

#define LV_LOG_ERROR(...) audio_stub_log(AUDIO_STUB_LOG_ERROR, __VA_ARGS__)
#define LV_LOG_WARN(...)  audio_stub_log(AUDIO_STUB_LOG_WARN, __VA_ARGS__)
#define LV_LOG_USER(...)  audio_stub_log(AUDIO_STUB_LOG_USER, __VA_ARGS__)
#define LV_LOG_INFO(...)  do { } while (0)
#define LV_LOG_TRACE(...) do { } while (0)

/* The opaque object type named by the neighbouring headers. */
typedef struct _lv_obj_t lv_obj_t;

/*
 *  Synthetic CW tone source for Auto Tune v2 (issue #190) in the host harness.
 *  See mocks/mock_cw_tune.h. It implements the DSP-side accessor the real
 *  firmware gets from src/cw.cpp, so the harness can run the whole session
 *  (COLLECT -> one correction -> VERIFY) without a radio and without DSP.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "mock_cw_tune.h"

#include <aether_radio/x6200_control/control.h>

#include "cw.h"
#include "cfg/cfg.h"
#include "util.h"

static bool             have_signal;
static float            tone_ratio;
static float            dds_offset_hz;
static uint64_t         sequence;
static uint64_t         frames;
static autotune_sample_t last_sample;

/* Modelo del receptor del banco (no codigo de produccion): el tono fisico es el
 * mismo y, al mover el dial, el error medido cambia como en la radio - en CW
 * subir el dial BAJA el tono de audio, en CWR (banda lateral invertida) lo SUBE.
 * Por eso `mock_cw_tune_set_tone()` declara el error EN EL DIAL DE ESE MOMENTO y
 * el error que se publica despues se recalcula con el dial actual: asi la
 * verificacion de `autotune-found` es un hecho (si la correccion de una sola
 * pasada es correcta, el error posterior queda ~0) y no un fixture que solo
 * repite el numero que se le dio. La relacion por banda lateral es la del
 * receptor y queda como requires-radio (docs/autotune.md). */
static int32_t ref_dial;
static float   ref_error;

void mock_cw_tune_reset(void) {
    have_signal = false;
    tone_ratio = 0.0f;
    dds_offset_hz = 0.0f;
    ref_dial = 0;
    ref_error = 0.0f;
    sequence = 0;
    frames = 0;
    last_sample = (autotune_sample_t) { 0 };
}

void mock_cw_tune_set_tone(float error_hz, float ratio) {
    have_signal = true;
    tone_ratio = ratio;
    ref_dial = subject_get_int(cfg_cur.fg_freq);
    ref_error = error_hz;
}

void mock_cw_tune_set_no_signal(void) {
    have_signal = false;
}

void mock_cw_tune_set_offset(float offset_hz) {
    dds_offset_hz = offset_hz;
}

uint64_t mock_cw_tune_frames(void) {
    return frames;
}

static float tone_error_now(void) {
    int32_t d = subject_get_int(cfg_cur.fg_freq) - ref_dial;

    return ref_error + (subject_get_int(cfg_cur.mode) == x6200_mode_cwr ? (float) d
                                                                       : -(float) d);
}

/* Cada consulta ve una trama NUEVA: secuencia estrictamente creciente y
 * timestamp del reloj virtual (el mismo get_time() de la ROM). Es la inyeccion
 * solo-en-tests de la instantanea del DSP. */
bool cw_get_tune_sample(autotune_sample_t *out) {
    if (out == NULL) {
        return false;
    }

    sequence++;
    frames++;
    last_sample.sequence = sequence;
    last_sample.timestamp_ms = get_time();
    last_sample.error_hz = tone_error_now();
    last_sample.ratio = tone_ratio;
    last_sample.dds_offset_hz = dds_offset_hz;
    last_sample.valid = have_signal;

    *out = last_sample;
    return true;
}

void cw_tune_note_dial_write(void) {
    /* En la radio este aviso hace que el hilo del DSP invalide su primera
     * ventana (puede traer audio de la frecuencia anterior). Aca no hay audio
     * atrasado: la frescura la dan la secuencia y el timestamp. No-op explicito,
     * no un olvido. */
}

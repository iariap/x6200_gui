/*
 *  TEST-ONLY spectrum source for Auto Tune multimode (issue #205) in the host
 *  harness. See mocks/mock_spectrum.h.
 *
 *  The bank has no DSP: src/dsp.cpp (the PSD that publishes the spectrum
 *  snapshot) is not compiled here, so a fixture that needs a carrier or a voice
 *  mass arms it explicitly instead of faking the production path. The injected
 *  snapshot carries the same fields the radio publishes (monotonic sequence,
 *  virtual-clock timestamp, epoch, center/mode/width from the live cfg_cur
 *  subjects, validity and the 800 dB bins), so the real coordinator and the real
 *  pure policies run exactly as on the radio.
 *
 *  Default state: NO signal (a flat noise floor), which is what the bank can
 *  honestly claim without arming anything.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "dsp.h"

#include <math.h>
#include <string.h>

#include "cfg/cfg.h"
#include "cfg/subjects.h"
#include "util.h"

#include "mock_spectrum.h"

static float spectrum_beta = 0.5f;

/* Modelo del receptor (no codigo de produccion): la portadora/voz es fisica y,
 * al mover el dial, cambia su offset respecto del centro - como en la radio. Un
 * fixture declara el offset EN EL DIAL DE ESE MOMENTO y cada publicacion lo
 * recalcula con el dial actual: por eso "la correccion de una sola pasada
 * converge" (o no) es un hecho del banco y no un numero repetido. */
static bool    have_carrier;
static bool    have_voice;
static float   ref_offset_hz;      /* portadora: offset; voz: centro de la masa */
static float   voice_width_hz;
static float   level_db;
static int32_t ref_dial_hz;

static float    noise_db = MOCK_SPECTRUM_DEFAULT_NOISE_DB;
static uint64_t sequence;
static uint64_t epoch = 1;
static uint64_t frames;

void mock_spectrum_reset(void) {
    have_carrier = false;
    have_voice = false;
    ref_offset_hz = 0.0f;
    voice_width_hz = 0.0f;
    level_db = 0.0f;
    ref_dial_hz = 0;
    noise_db = MOCK_SPECTRUM_DEFAULT_NOISE_DB;
    sequence = 0;
    epoch = 1;
    frames = 0;
}

void mock_spectrum_set_noise(float db) {
    noise_db = db;
}

void mock_spectrum_set_carrier(float offset_hz, float level_db_in) {
    have_carrier = true;
    have_voice = false;
    ref_offset_hz = offset_hz;
    level_db = level_db_in;
    ref_dial_hz = subject_get_int(cfg_cur.fg_freq);
}

void mock_spectrum_set_voice(float center_hz, float width_hz, float level_db_in) {
    have_voice = true;
    have_carrier = false;
    ref_offset_hz = center_hz;
    voice_width_hz = width_hz;
    level_db = level_db_in;
    ref_dial_hz = subject_get_int(cfg_cur.fg_freq);
}

uint64_t mock_spectrum_frames(void) {
    return frames;
}

uint64_t mock_spectrum_epoch(void) {
    return epoch;
}

/* Bin de display mas cercano al offset pedido (el frame interpola 512 bins
 * fisicos en 800 de display, como la radio). */
static int32_t bin_of(float offset_hz, int32_t width_hz) {
    float step = (float) width_hz / (float) AUTOTUNE_SPECTRUM_BINS;
    int32_t bin = AUTOTUNE_SPECTRUM_CENTER + (int32_t) lroundf(offset_hz / step);

    if (bin < 0) {
        bin = 0;
    }
    if (bin >= AUTOTUNE_SPECTRUM_BINS) {
        bin = AUTOTUNE_SPECTRUM_BINS - 1;
    }
    return bin;
}

void dsp_autotune_invalidate(void) {
    epoch++;
}

bool dsp_autotune_get_spectrum(dsp_autotune_spectrum_t *out) {
    int32_t width = subject_get_int(cfg_cur.fft_width);
    int32_t dial = subject_get_int(cfg_cur.fg_freq);
    float   offset_now = ref_offset_hz - (float) (dial - ref_dial_hz);

    if (out == NULL) {
        return false;
    }

    sequence++;
    frames++;

    out->sequence = sequence;
    out->timestamp_ms = get_time();
    out->epoch = epoch;
    out->center_freq_hz = dial;
    out->mode = subject_get_int(cfg_cur.mode);
    out->width_hz = width;
    out->tx = false;
    out->valid = (width > 0);

    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        out->db[i] = noise_db;
    }
    if (width > 0 && have_carrier) {
        out->db[bin_of(offset_now, width)] = level_db;
    }
    if (width > 0 && have_voice) {
        float step = (float) width / (float) AUTOTUNE_SPECTRUM_BINS;

        for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
            float off = (float) (i - AUTOTUNE_SPECTRUM_CENTER) * step;

            if (fabsf(off - offset_now) <= voice_width_hz / 2.0f) {
                out->db[i] = level_db;
            }
        }
    }
    return true;
}

/* --- DSP surface the rendered components still call ------------------------- */

void dsp_set_spectrum_beta(float x)
{
    spectrum_beta = x;
}

float dsp_get_spectrum_beta(void)
{
    return spectrum_beta;
}

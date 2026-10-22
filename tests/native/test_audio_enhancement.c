/*
 * Unit tests: audio enhancement chain.
 *
 * Covers the 2026-10-11 fixes:
 *  - full blocks (~4410 samples from the PulseAudio callback) must be processed
 *    (before the fix anything larger than 256 samples was silently dropped)
 *  - noise reduction only touches complete AUDIO_ENH_FRAME_SIZE frames
 *  - presets out of range are clamped
 *  - the AF leveler cannot produce out-of-range samples
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "audio_enhancement.h"
#include "test_framework.h"

void test_tick_set(uint32_t ms);
int  test_speex_frames(void);

#define BLOCK   4410            /* 100 ms @ 44.1 kHz, as delivered by audio.c */
#define FRAME   AUDIO_ENH_FRAME_SIZE

static void fill_const(int16_t *buf, size_t n, int16_t v) {
    for (size_t i = 0; i < n; i++) {
        buf[i] = v;
    }
}

static size_t count_changed(const int16_t *a, const int16_t *b, size_t n) {
    size_t changed = 0;

    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            changed++;
        }
    }

    return changed;
}

/* OFF must be a bit-exact pass-through (no CPU cost). */
static void test_off_is_passthrough(void) {
    int16_t in[BLOCK], out[BLOCK];

    fill_const(in, BLOCK, 1234);
    memcpy(out, in, sizeof(in));

    audio_enh_preset_load(AUDIO_ENH_PRESET_OFF);

    CHECK(!audio_enh_is_enabled());

    audio_enh_process(out, BLOCK);

    CHECK(memcmp(in, out, sizeof(in)) == 0);
}

/* Out-of-range stored values must not leave the chain in a bogus state. */
static void test_preset_out_of_range_is_clamped(void) {
    audio_enh_preset_load((audio_enh_preset_t) 99);

    CHECK(audio_enh_preset_get_current() == AUDIO_ENH_PRESET_OFF);
    CHECK(!audio_enh_is_enabled());
}

/*
 * Regression test for the main bug: the capture callback delivers ~4410
 * samples and the old guard `nsamples > AUDIO_ENH_FRAME_SIZE` discarded the
 * whole block, so the feature never ran.
 */
static void test_full_block_is_processed(void) {
    int16_t in[BLOCK], out[BLOCK];

    fill_const(in, BLOCK, 2000);
    memcpy(out, in, sizeof(in));

    audio_enh_preset_load(AUDIO_ENH_PRESET_CW_WEAK);   /* AF leveler on, NR off */

    CHECK(audio_enh_is_enabled());

    audio_enh_process(out, BLOCK);

    /* Every sample must have been through the chain, including the tail chunk
     * (4410 = 17 * 256 + 58). */
    CHECK(count_changed(in, out, BLOCK) == BLOCK);
}

/* The same buffer processed twice must give the same result (predictable). */
static void test_deterministic(void) {
    int16_t in[512], a[512], b[512];

    for (int i = 0; i < 512; i++) {
        in[i] = (int16_t) (1000.0 * sin(i * 0.1));
    }

    audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);   /* NR only, stateless */

    memcpy(a, in, sizeof(in));
    memcpy(b, in, sizeof(in));

    audio_enh_process(a, 512);
    audio_enh_process(b, 512);

    CHECK(memcmp(a, b, sizeof(a)) == 0);
}

/* Noise reduction runs on complete frames only; a partial tail passes through. */
static void test_nr_only_full_frames(void) {
    int16_t in[300], out[300];
    int     frames_before;

    fill_const(in, 300, 1000);
    memcpy(out, in, sizeof(in));

    audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);

    frames_before = test_speex_frames();
    audio_enh_process(out, 300);

    CHECK(test_speex_frames() - frames_before == 1);    /* exactly one 256 frame */

    int processed_bad = 0;

    for (int i = 0; i < FRAME; i++) {
        int expected = in[i] / 2;                       /* stub attenuates 6 dB */

        if (abs(out[i] - expected) > 2) {
            processed_bad++;
        }
    }

    CHECK(processed_bad == 0);

    int tail_bad = 0;

    for (int i = FRAME; i < 300; i++) {
        if (out[i] != in[i]) {
            tail_bad++;
        }
    }

    CHECK(tail_bad == 0);
}

/* Frame boundaries: 256 -> 1 frame, 257 -> 1 frame + untouched tail. */
static void test_frame_boundaries(void) {
    int16_t in[257], out[257];
    int     frames_before;

    fill_const(in, 257, 800);
    memcpy(out, in, sizeof(in));
    audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);

    frames_before = test_speex_frames();
    audio_enh_process(out, FRAME);
    CHECK(test_speex_frames() - frames_before == 1);

    memcpy(out, in, sizeof(in));
    frames_before = test_speex_frames();
    audio_enh_process(out, 257);
    CHECK(test_speex_frames() - frames_before == 1);
    CHECK(out[256] == in[256]);
}

/* The AF leveler must clamp its gain (no wraparound / clipping garbage). */
static void test_leveler_gain_is_bounded(void) {
    int16_t in[BLOCK], out[BLOCK];
    int     max_out = 0;

    fill_const(in, BLOCK, 1);       /* very weak signal: would want +65 dB */
    memcpy(out, in, sizeof(in));

    audio_enh_preset_load(AUDIO_ENH_PRESET_CW_WEAK);
    audio_enh_process(out, BLOCK);

    for (size_t i = 0; i < BLOCK; i++) {
        int v = out[i] < 0 ? -out[i] : out[i];

        if (v > max_out) {
            max_out = v;
        }
    }

    CHECK(max_out > 1);             /* gain was applied */
    CHECK(max_out <= 40);           /* ...but clamped to +30 dB (31.6x) */
}

/* Notch API sanity (liquid-dsp stub is a pass-through). */
static void test_notches(void) {
    audio_enh_notch_clear_all();
    CHECK(audio_enh_get_active_notch_count() == 0);

    int id = audio_enh_notch_add(1500.0f, 100.0f);

    CHECK(id == 0);
    CHECK(audio_enh_notch_is_active(id));
    CHECK(audio_enh_get_active_notch_count() == 1);

    audio_enh_notch_remove(id);
    CHECK(audio_enh_get_active_notch_count() == 0);

    /* Fill all slots, then confirm the fifth request fails cleanly. */
    for (int i = 0; i < AUDIO_ENH_MAX_NOTCHES; i++) {
        CHECK(audio_enh_notch_add(1000.0f + 100.0f * i, 50.0f) == i);
    }

    CHECK(audio_enh_notch_add(2000.0f, 50.0f) == -1);

    audio_enh_notch_clear_all();
    CHECK(audio_enh_get_active_notch_count() == 0);
}

void test_audio_enhancement(void) {
    audio_enh_init();

    test_off_is_passthrough();
    test_preset_out_of_range_is_clamped();
    test_full_block_is_processed();
    test_deterministic();
    test_nr_only_full_frames();
    test_frame_boundaries();
    test_leveler_gain_is_bounded();
    test_notches();
}

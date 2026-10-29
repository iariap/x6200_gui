/*
 * Unit tests: audio enhancement chain (capture copy only).
 *
 * Covers issue #351, part 1:
 *  - the module initializes SpeexDSP with the REAL capture rate (44100 Hz) and a
 *    10 ms frame (441 samples); the stub fails the test if it does not
 *  - the NR frame machine is stateful and continuous: exactly N output samples per
 *    N input samples, one 441-sample priming once, no raw tail per callback
 *  - the same stream delivered in different partitions produces BIT-IDENTICAL output
 *    for SSB voice (NR on, AF leveler off), Off and Digital
 *  - AM broadcast (AF leveler on) is NOT partition-invariant by design: the leveler
 *    measures its peak once per block, so only length/continuity/bounds are required
 *  - resets (Off<->On, preset change, hole) never replay old frames
 *  - the 0..100 level maps to the Speex polarity: 0->-10, 40->-18, 60->-22, 100->-30
 *  - CW/CWR keeps the NR off (PR #232) without touching the stored preset
 *
 * These tests run against a STUB that just halves every sample: they prove framing,
 * ordering and wiring, NOT acoustic quality, attenuation, CPU cost or latency.
 * Any claim about how the radio sounds needs the real SpeexDSP bench and the radio.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>
#include <string.h>

#include <aether_radio/x6200_control/control.h>

#include "audio_enhancement.h"
#include "test_framework.h"

int  test_speex_frames(void);
int  test_speex_init_count(void);
int  test_speex_init_frame_size(void);
int  test_speex_init_rate(void);
int  test_speex_init_mismatch(void);
int  test_speex_noise_suppress(void);
int  test_speex_noise_suppress_calls(void);
void test_speex_reset_counters(void);
void test_audio_enh_mode_set(int32_t mode);
void test_audio_enh_stored_preset_set(uint8_t preset);
uint8_t test_audio_enh_stored_preset_get(void);

#define BLOCK   4410            /* 100 ms @ 44.1 kHz, as delivered by audio.c */
#define FRAME   AUDIO_ENH_FRAME_SIZE

/* Deterministic, varied stream: never zero, so a dropped/reordered sample is visible and
 * the stub's /2 cannot turn a sample into an unrelated 0. */
static void fill_stream(int16_t *buf, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int32_t v = 2000 + (int32_t) ((i * 37u) % 26000u);

        buf[i] = (int16_t) ((i & 1u) ? -v : v);
    }
}

static void fill_const(int16_t *buf, size_t n, int16_t v) {
    for (size_t i = 0; i < n; i++) {
        buf[i] = v;
    }
}

/* Deja constancia de que la particion cubre el stream completo (evita un test que "pasa"
 * porque le faltaban muestras por entregar). */
static size_t sum_parts(const size_t *parts, size_t nparts) {
    size_t total = 0;

    for (size_t p = 0; p < nparts; p++) {
        total += parts[p];
    }

    return total;
}

/* Corre el stream `in` por la cadena en el orden exacto de `parts` (cuya suma es n) y deja
 * el resultado en `out`. Cada corrida parte de audio_enh_init() + preset, o sea del mismo
 * estado inicial que tendria la radio al arrancar. */
static void run_partitioned(audio_enh_preset_t preset, const int16_t *in, int16_t *out,
                            size_t n, const size_t *parts, size_t nparts) {
    audio_enh_init();
    audio_enh_preset_load(preset);

    size_t off = 0;

    for (size_t p = 0; p < nparts && off < n; p++) {
        size_t len = parts[p];

        if (len > n - off) {
            len = n - off;
        }

        memcpy(out + off, in + off, len * sizeof(int16_t));
        audio_enh_process(out + off, len);
        off += len;
    }

    /* Tail de la particion (si `parts` no cubre n): una sola entrega. */
    if (off < n) {
        memcpy(out + off, in + off, (n - off) * sizeof(int16_t));
        audio_enh_process(out + off, n - off);
    }
}

/* OFF must be a bit-exact pass-through (no CPU cost). */
static void test_off_is_passthrough(void) {
    int16_t in[BLOCK], out[BLOCK];

    fill_const(in, BLOCK, 1234);
    memcpy(out, in, sizeof(in));

    audio_enh_init();
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

/* SpeexDSP must be created with the capture rate and a 10 ms frame, once per init. */
static void test_speex_init_contract(void) {
    CHECK(AUDIO_ENH_SAMPLE_RATE == 44100);
    CHECK(AUDIO_ENH_FRAME_SIZE == 441);

    test_speex_reset_counters();
    audio_enh_init();

    CHECK(test_speex_init_count() == 1);
    CHECK(test_speex_init_frame_size() == 441);
    CHECK(test_speex_init_rate() == 44100);
    CHECK(test_speex_init_mismatch() == 0);
}

/*
 * Regression test for the old `floor(n/256) + raw tail` policy: with CW weak (AF leveler
 * on, NR off) every delivered sample must still go through the chain.
 */
static void test_full_block_is_processed(void) {
    int16_t in[BLOCK], out[BLOCK];

    fill_const(in, BLOCK, 2000);
    memcpy(out, in, sizeof(in));

    audio_enh_init();
    audio_enh_preset_load(AUDIO_ENH_PRESET_CW_WEAK);   /* AF leveler on, NR off */

    CHECK(audio_enh_is_enabled());

    audio_enh_process(out, BLOCK);

    /* Every sample must have been through the chain (4410 is not a multiple of the old
     * frame size either: the raw tail is gone). */
    size_t changed = 0;

    for (size_t i = 0; i < BLOCK; i++) {
        if (in[i] != out[i]) changed++;
    }

    CHECK(changed == BLOCK);
}

/* The same fresh state + same preset + same stream must give the same output. */
static void test_deterministic(void) {
    int16_t in[512], a[512], b[512];

    /* SSB voice: a stream that never maps to 0 after the stub's /2, so a zero in the
     * output always means "priming/old state", never "this sample happened to be 0". */
    fill_stream(in, 512);

    run_partitioned(AUDIO_ENH_PRESET_SSB_VOICE, in, a, 512, (const size_t[]) {512}, 1);
    run_partitioned(AUDIO_ENH_PRESET_SSB_VOICE, in, b, 512, (const size_t[]) {512}, 1);

    CHECK(memcmp(a, b, sizeof(a)) == 0);
}

/*
 * The contract of issue #351 section B: N samples in, N samples out, exactly one
 * 441-sample priming, no raw tail. With the old implementation out[0] was already
 * processed (no priming) and the last samples of each 256-remainder went through raw.
 */
static void test_priming_and_no_raw_tail(void) {
    int16_t in[BLOCK], out[BLOCK];
    int     frames_before;

    fill_const(in, BLOCK, 1000);
    memcpy(out, in, sizeof(in));

    audio_enh_init();
    audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);

    frames_before = test_speex_frames();
    audio_enh_process(out, BLOCK);

    /* Exactly one priming frame of silence at the start... */
    for (size_t i = 0; i < FRAME; i++) {
        CHECK(out[i] == 0);
    }

    /* ...and after it every sample is the processed (stubbed /2) one: no sample of the
     * stream is ever emitted raw. */
    int raw = 0;

    for (size_t i = FRAME; i < BLOCK; i++) {
        if (out[i] != 500) raw++;
    }

    CHECK(raw == 0);

    /* 4410 samples = exactly 10 complete Speex frames, none processed twice. */
    CHECK(test_speex_frames() - frames_before == 10);
}

/* Small/boundary sizes: output length is always the input length, frames are only counted
 * for complete 441-sample groups and the priming happens once per startup. */
static void test_frame_boundaries(void) {
    static const size_t sizes[] = {1, 440, 441, 442, 257, 4410, 8820};
    static const int    frames[] = {0, 0, 1, 1, 0, 10, 20};
    static const size_t priming[] = {1, 440, 441, 441, 257, 441, 441};

    for (size_t c = 0; c < sizeof(sizes) / sizeof(sizes[0]); c++) {
        int16_t in[8820], out[8820];
        size_t  n = sizes[c];

        fill_stream(in, n);
        memcpy(out, in, n * sizeof(int16_t));

        audio_enh_init();
        audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);

        int frames_before = test_speex_frames();

        audio_enh_process(out, n);

        CHECK(test_speex_frames() - frames_before == frames[c]);

        /* Priming = min(n, 441) leading zeros, never more. */
        for (size_t i = 0; i < priming[c]; i++) {
            CHECK(out[i] == 0);
        }

        if (n > priming[c]) {
            CHECK(out[priming[c]] != 0);
        }
    }
}

/* SSB voice (NR on, leveler off): the same stream cut differently must give byte-identical
 * output, including across a callback boundary in the middle of a Speex frame. */
static void test_ssb_partition_invariance(void) {
    int16_t in[8820], ref[8820], got[8820];

    fill_stream(in, sizeof(in) / sizeof(in[0]));

    /* 8820 = 20 tramas de 441. */
    static const size_t one[] = {8820};

    static const size_t ten[] = {
        441, 441, 441, 441, 441, 441, 441, 441, 441, 441,
        441, 441, 441, 441, 441, 441, 441, 441, 441, 441,
    };

    /* 1 + 440 + 19 * 441: la primera trama (y el priming) se arma con dos entregas. */
    static const size_t odd[] = {
        1, 440, 441, 441, 441, 441, 441, 441, 441, 441,
        441, 441, 441, 441, 441, 441, 441, 441, 441, 441, 441,
    };

    /* 257 + 154 + 9 * 441, dos veces, + 60: cada bloque de 411 corta una trama al medio. */
    static const size_t ragged[] = {
        257, 154, 441, 441, 441, 441, 441, 441, 441, 441, 441,
        257, 154, 441, 441, 441, 441, 441, 441, 441, 441, 441,
        60,
    };

    CHECK(sum_parts(one, sizeof(one) / sizeof(one[0])) == 8820);
    CHECK(sum_parts(ten, sizeof(ten) / sizeof(ten[0])) == 8820);
    CHECK(sum_parts(odd, sizeof(odd) / sizeof(odd[0])) == 8820);
    CHECK(sum_parts(ragged, sizeof(ragged) / sizeof(ragged[0])) == 8820);

    run_partitioned(AUDIO_ENH_PRESET_SSB_VOICE, in, ref, 8820, one,
                    sizeof(one) / sizeof(one[0]));

    run_partitioned(AUDIO_ENH_PRESET_SSB_VOICE, in, got, 8820, ten,
                    sizeof(ten) / sizeof(ten[0]));
    CHECK(memcmp(ref, got, sizeof(ref)) == 0);

    run_partitioned(AUDIO_ENH_PRESET_SSB_VOICE, in, got, 8820, odd,
                    sizeof(odd) / sizeof(odd[0]));
    CHECK(memcmp(ref, got, sizeof(ref)) == 0);

    run_partitioned(AUDIO_ENH_PRESET_SSB_VOICE, in, got, 8820, ragged,
                    sizeof(ragged) / sizeof(ragged[0]));
    CHECK(memcmp(ref, got, sizeof(ref)) == 0);
}

/* Off and Digital are passthrough: byte-identical for ANY partition and with no priming
 * (the first sample delivered is the first sample that comes out). */
static void test_off_and_digital_bit_exact_any_partition(void) {
    static const audio_enh_preset_t presets[] = {
        AUDIO_ENH_PRESET_OFF, AUDIO_ENH_PRESET_DIGITAL
    };
    int16_t in[8820], out[8820];

    fill_stream(in, sizeof(in) / sizeof(in[0]));

    for (size_t p = 0; p < sizeof(presets) / sizeof(presets[0]); p++) {
        /* One delivery. */
        run_partitioned(presets[p], in, out, 8820, (const size_t[]) {8820}, 1);
        CHECK(memcmp(in, out, sizeof(in)) == 0);

        /* Ten 441-sample callbacks. */
        static const size_t ten[] = {441, 441, 441, 441, 441,
                                     441, 441, 441, 441, 441,
                                     441, 441, 441, 441, 441,
                                     441, 441, 441, 441, 441};

        run_partitioned(presets[p], in, out, 8820, ten, sizeof(ten) / sizeof(ten[0]));
        CHECK(memcmp(in, out, sizeof(in)) == 0);

        /* Single-sample delivery for the first millisecond, then ragged blocks. */
        static const size_t ragged[] = {1, 1, 1, 440, 441, 257, 154, 441, 441, 441,
                                        441, 441, 441, 441, 441, 441, 441, 441, 441,
                                        441, 441, 441, 441, 441, 441, 1};

        run_partitioned(presets[p], in, out, 8820, ragged,
                        sizeof(ragged) / sizeof(ragged[0]));
        CHECK(memcmp(in, out, sizeof(in)) == 0);
    }
}

/* AM broadcast keeps its AF leveler, which measures the peak once per delivered block: the
 * output is NOT required to match across partitions, but it must keep length, never clip
 * into wraparound and keep the same number of samples/frames. */
static void test_am_length_continuity_and_bounds(void) {
    int16_t in[8820], ref[8820], got[8820];
    int     frames_ref = 0, frames_got = 0;

    fill_stream(in, sizeof(in) / sizeof(in[0]));

    int before = test_speex_frames();

    run_partitioned(AUDIO_ENH_PRESET_AM_BROADCAST, in, ref, 8820,
                    (const size_t[]) {8820}, 1);
    frames_ref = test_speex_frames() - before;

    static const size_t ragged[] = {
        257, 154, 441, 441, 441, 441, 441, 441, 441, 441, 441,
        257, 154, 441, 441, 441, 441, 441, 441, 441, 441, 441,
        60,
    };

    CHECK(sum_parts(ragged, sizeof(ragged) / sizeof(ragged[0])) == 8820);

    before = test_speex_frames();

    run_partitioned(AUDIO_ENH_PRESET_AM_BROADCAST, in, got, 8820, ragged,
                    sizeof(ragged) / sizeof(ragged[0]));
    frames_got = test_speex_frames() - before;

    /* Same stream, same number of complete frames regardless of the partition. */
    CHECK(frames_ref == 20);
    CHECK(frames_got == 20);

    /* Both runs start with the 441-sample priming and then produce something for every
     * sample (the leveler never leaves a dead block). */
    size_t silent_ref = 0, silent_got = 0;

    for (size_t i = FRAME; i < 8820; i++) {
        if (ref[i] == 0) silent_ref++;
        if (got[i] == 0) silent_got++;
    }

    CHECK(silent_ref == 0);
    CHECK(silent_got == 0);

    for (size_t i = 0; i < 8820; i++) {
        CHECK(got[i] != 32767);     /* no saturation plateau from gain wraparound */
        CHECK(got[i] != -32768);
    }
}

/* The stub attenuates 6 dB, so SSB voice output is the halved stream after the priming;
 * a restart must not leak the previous run's samples into the new one. */
static void test_reset_discards_pending_audio(void) {
    int16_t in[FRAME], out[FRAME];

    fill_const(in, FRAME, 4000);

    audio_enh_init();
    audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);

    /* First frame: priming only (nothing processed yet is emitted). */
    memcpy(out, in, sizeof(in));
    audio_enh_process(out, FRAME);

    for (size_t i = 0; i < FRAME; i++) {
        CHECK(out[i] == 0);
    }

    /* The hole of the stream (or any explicit reset) throws the pending frame away: the
     * next frame starts from scratch, it is not the tail of the previous audio. */
    audio_enh_nr_reset_pending();

    memcpy(out, in, sizeof(in));
    audio_enh_process(out, FRAME);

    for (size_t i = 0; i < FRAME; i++) {
        CHECK(out[i] == 0);
    }

    /* Now that a full frame exists, its processed copy IS emitted (proves the machine is
     * alive and only the discarded frame was lost). */
    memcpy(out, in, sizeof(in));
    audio_enh_process(out, FRAME);

    for (size_t i = 0; i < FRAME; i++) {
        CHECK(out[i] == in[i] / 2);
    }
}

/* Off<->On, preset changes and Digital must reset the machine: no old sample survives. */
static void test_preset_and_mode_switch_reset(void) {
    int16_t in[FRAME], out[FRAME];

    fill_const(in, FRAME, 4000);

    audio_enh_init();
    audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);

    memcpy(out, in, sizeof(in));
    audio_enh_process(out, FRAME);              /* frame pending, nothing emitted yet */

    /* OFF: passthrough while the preset is off... */
    audio_enh_preset_load(AUDIO_ENH_PRESET_OFF);
    memcpy(out, in, sizeof(in));
    audio_enh_process(out, FRAME);
    CHECK(memcmp(in, out, sizeof(in)) == 0);

    /* ...and back to SSB voice starts from the priming again, never from the old frame. */
    audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);
    memcpy(out, in, sizeof(in));
    audio_enh_process(out, FRAME);

    for (size_t i = 0; i < FRAME; i++) {
        CHECK(out[i] == 0);
    }

    /* Digital also resets and is passthrough even right after a pending NR frame. */
    memcpy(out, in, sizeof(in));
    audio_enh_process(out, FRAME);              /* pending frame for SSB */
    audio_enh_preset_load(AUDIO_ENH_PRESET_DIGITAL);
    memcpy(out, in, sizeof(in));
    audio_enh_process(out, FRAME);
    CHECK(memcmp(in, out, sizeof(in)) == 0);
}

/* CW weak: NR stays disabled (no priming, no Speex calls) and the AF leveler keeps
 * working, exactly as the PR #232 profile requires. */
static void test_cw_weak_has_no_nr(void) {
    int16_t in[BLOCK], out[BLOCK];
    int     frames_before;

    fill_const(in, BLOCK, 1);       /* very weak: the leveler wants a lot of gain */
    memcpy(out, in, sizeof(in));

    audio_enh_init();
    audio_enh_preset_load(AUDIO_ENH_PRESET_CW_WEAK);

    frames_before = test_speex_frames();
    audio_enh_process(out, BLOCK);

    CHECK(test_speex_frames() == frames_before);    /* CW never runs Speex */
    CHECK(out[0] != 0);                             /* no priming */
    CHECK(out[BLOCK - 1] != 0);                     /* no tail dropped either */
}

/* The AF leveler must clamp its gain (no wraparound / clipping garbage). */
static void test_leveler_gain_is_bounded(void) {
    int16_t in[BLOCK], out[BLOCK];
    int     max_out = 0;

    fill_const(in, BLOCK, 1);       /* very weak signal: would want +65 dB */
    memcpy(out, in, sizeof(in));

    audio_enh_init();
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

/* Speex expects a NEGATIVE suppression: more negative = more attenuation. The preset
 * levels stay 40/60; only the mapping is corrected (0->-10, 40->-18, 60->-22, 100->-30). */
static void test_nr_level_polarity(void) {
    audio_enh_init();

    audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);
    CHECK(test_speex_noise_suppress() == -18);

    audio_enh_preset_load(AUDIO_ENH_PRESET_AM_BROADCAST);
    CHECK(test_speex_noise_suppress() == -22);

    /* The preset stored values did not change: level is still the 0..100 scale. */
    CHECK(g_audio_enh.nr.level == 60);

    audio_enh_nr_set_level(0);
    CHECK(test_speex_noise_suppress() == -10);

    audio_enh_nr_set_level(100);
    CHECK(test_speex_noise_suppress() == -30);

    /* Out-of-range input is clamped, not wrapped. */
    audio_enh_nr_set_level(200);
    CHECK(g_audio_enh.nr.level == 100);
    CHECK(test_speex_noise_suppress() == -30);

    /* No spurious ctl: every assertion above went through exactly one call. */
    CHECK(test_speex_noise_suppress_calls() > 0);
}

/* CW/CWR takes the CW profile (no Speex NR) without rewriting the user's stored preset;
 * OFF/DIGITAL are explicit "do not process" and are honoured in CW too (PR #232). */
static void test_cw_mode_preset_keeps_stored_value(void) {
    test_audio_enh_stored_preset_set(AUDIO_ENH_PRESET_SSB_VOICE);
    test_audio_enh_mode_set(x6200_mode_cw);

    audio_enh_init();
    audio_enh_apply_mode_preset();

    CHECK(audio_enh_preset_get_current() == AUDIO_ENH_PRESET_CW_WEAK);
    CHECK(!audio_enh_nr_is_enabled());
    CHECK(test_audio_enh_stored_preset_get() == AUDIO_ENH_PRESET_SSB_VOICE);

    test_audio_enh_stored_preset_set(AUDIO_ENH_PRESET_OFF);
    audio_enh_apply_mode_preset();
    CHECK(audio_enh_preset_get_current() == AUDIO_ENH_PRESET_OFF);

    test_audio_enh_stored_preset_set(AUDIO_ENH_PRESET_DIGITAL);
    audio_enh_apply_mode_preset();
    CHECK(audio_enh_preset_get_current() == AUDIO_ENH_PRESET_DIGITAL);

    /* Outside CW (and CWR) the stored preset applies as-is. */
    test_audio_enh_stored_preset_set(AUDIO_ENH_PRESET_SSB_VOICE);
    test_audio_enh_mode_set(x6200_mode_usb);
    audio_enh_apply_mode_preset();
    CHECK(audio_enh_preset_get_current() == AUDIO_ENH_PRESET_SSB_VOICE);
    CHECK(audio_enh_nr_is_enabled());

    /* CWR behaves like CW. */
    test_audio_enh_mode_set(x6200_mode_cwr);
    audio_enh_apply_mode_preset();
    CHECK(audio_enh_preset_get_current() == AUDIO_ENH_PRESET_CW_WEAK);
    CHECK(!audio_enh_nr_is_enabled());
    CHECK(test_audio_enh_stored_preset_get() == AUDIO_ENH_PRESET_SSB_VOICE);

    /* Estado del stub de vuelta al default (x6200_mode_lsb) para el resto de la suite. */
    test_audio_enh_mode_set(x6200_mode_lsb);
    test_audio_enh_stored_preset_set(AUDIO_ENH_PRESET_OFF);
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
    test_speex_init_contract();
    test_off_is_passthrough();
    test_preset_out_of_range_is_clamped();
    test_full_block_is_processed();
    test_deterministic();
    test_priming_and_no_raw_tail();
    test_frame_boundaries();
    test_ssb_partition_invariance();
    test_off_and_digital_bit_exact_any_partition();
    test_am_length_continuity_and_bounds();
    test_reset_discards_pending_audio();
    test_preset_and_mode_switch_reset();
    test_cw_weak_has_no_nr();
    test_leveler_gain_is_bounded();
    test_nr_level_polarity();
    test_cw_mode_preset_keeps_stored_value();
    test_notches();
}

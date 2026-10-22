/*
 * Unit tests: the CW decoder auto-tune core (issue #139).
 *
 * The module takes the statistics src/cw.cpp measures per window and returns
 * the four decoder parameters plus the DDS correction. Everything here is a
 * synthetic vector: no radio, no LVGL, no liquid-dsp. The expected numbers are
 * derived in docs/cw139-spec.md (the case table); each assertion names the
 * formula it pins.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <string.h>

#include "cw_autotune.h"
#include "test_framework.h"

/* A window with signal: 40 dB of mark over the floor, a stable floor and a
 * 20 WPM fist. `peak` is the measured spectral offset (see HALLAZGOS 52). */
static cw_autotune_stats_t strong(float peak) {
    cw_autotune_stats_t s;

    memset(&s, 0, sizeof(s));
    s.noise_db = -80.0f;
    s.mark_db = -40.0f;
    s.noise_var_db2 = 0.25f;
    s.mark_var_db2 = 1.0f;
    s.signal_ratio = 50.0f;
    s.block_dt_s = 0.18576f;    /* 128 muestras a 689,0625 Hz (src/cw.cpp) */
    s.peak_freq_hz = peak;
    s.wpm = 20;
    s.mark_valid = true;
    s.peak_valid = true;
    return s;
}

/* --- tau and window statistics ------------------------------------------- */

static void test_tau_peak_by_wpm(void) {
    /* Half a dot: a dot is 1200/WPM ms (PARIS), so tau = 600/WPM ms. */
    CHECK_CLOSE(cw_autotune_tau_peak_ms(10), 60.0f, 0.001f);
    CHECK_CLOSE(cw_autotune_tau_peak_ms(20), 30.0f, 0.001f);
    CHECK_CLOSE(cw_autotune_tau_peak_ms(30), 20.0f, 0.001f);
    CHECK_CLOSE(cw_autotune_tau_peak_ms(60), 10.0f, 0.001f);

    /* Clamped to the decodable range; 0 (not measured yet) uses 20 WPM. */
    CHECK_CLOSE(cw_autotune_tau_peak_ms(1), 120.0f, 0.001f);
    CHECK_CLOSE(cw_autotune_tau_peak_ms(200), 10.0f, 0.001f);
    CHECK_CLOSE(cw_autotune_tau_peak_ms(0), 30.0f, 0.001f);
    CHECK_CLOSE(cw_autotune_tau_peak_ms(-5), 30.0f, 0.001f);
}

static void test_percentile_nearest_rank(void) {
    const float v[] = { 5.0f, 1.0f, 9.0f, 2.0f, 10.0f, 3.0f, 8.0f, 4.0f, 7.0f, 6.0f };

    /* Nearest rank: q=0,9 over ten values is the ninth (sorted) value. */
    CHECK_CLOSE(cw_autotune_percentile(v, 10, 0.9f), 9.0f, 0.001f);
    CHECK_CLOSE(cw_autotune_percentile(v, 10, 0.5f), 5.0f, 0.001f);
    CHECK_CLOSE(cw_autotune_percentile(v, 10, 0.0f), 1.0f, 0.001f);
    CHECK_CLOSE(cw_autotune_percentile(v, 10, 1.0f), 10.0f, 0.001f);

    /* Degenerate windows answer 0 instead of reading garbage. */
    CHECK_CLOSE(cw_autotune_percentile(v, 0, 0.9f), 0.0f, 0.0001f);
    CHECK_CLOSE(cw_autotune_percentile(NULL, 4, 0.9f), 0.0f, 0.0001f);
}

static void test_mean_and_variance(void) {
    const float v[] = { 1.0f, 2.0f, 3.0f, 4.0f };

    CHECK_CLOSE(cw_autotune_mean(v, 4), 2.5f, 0.001f);
    /* Population variance: ((1.5)^2 + (0.5)^2 + (0.5)^2 + (1.5)^2)/4 = 1.25. */
    CHECK_CLOSE(cw_autotune_variance(v, 4), 1.25f, 0.001f);
    CHECK_CLOSE(cw_autotune_mean(v, 0), 0.0f, 0.0001f);
    CHECK_CLOSE(cw_autotune_variance(v, 0), 0.0f, 0.0001f);
}

/* --- the four parameters -------------------------------------------------- */

static void test_strong_signal(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s = strong(100.0f);   /* tone 100 Hz below the DDS centre */

    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);

    CHECK(r.adapt);
    /* snr = clamp(0.5 * (mark - noise), 3, 10) = clamp(20, 3, 10) = 10. */
    CHECK_CLOSE(r.snr, 10.0f, 0.001f);
    /* gist = clamp(0.25 * snr, 0.5, 3) = 2.5. */
    CHECK_CLOSE(r.gist, 2.5f, 0.001f);
    /* peak_beta = clamp(1 - block/tau, .10, .95), tau = 30 ms, block = 185.76:
     * 1 - 6.19 = -5.19 -> the lower clamp. See the table in the spec: with the
     * real FFT block every WPM in 5..60 lands on the clamp (HALLAZGOS 53). */
    CHECK_CLOSE(r.peak_beta, 0.10f, 0.001f);
    /* A stable floor (sigma 0.5 dB) -> noise_beta = 0.90 (stable band). */
    CHECK_CLOSE(r.noise_beta, 0.90f, 0.001f);
    /* First window: every parameter is applied, including the tone. */
    CHECK(r.apply_snr && r.apply_gist && r.apply_peak_beta && r.apply_noise_beta && r.apply_freq);
    /* freq = -peak, slewed to one step (-30 Hz) on the first window. */
    CHECK_CLOSE(r.freq_offset_hz, -30.0f, 0.001f);
}

static void test_weak_signal(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s;

    memset(&s, 0, sizeof(s));
    s.noise_db = -80.0f;
    s.mark_db = -73.0f;          /* 7 dB of mark over the floor */
    s.noise_var_db2 = 1.0f;      /* sigma 1 dB: still the stable band */
    s.mark_var_db2 = 0.5f;
    s.signal_ratio = 3.0f;
    s.block_dt_s = 0.18576f;
    s.peak_freq_hz = 20.0f;
    s.wpm = 20;
    s.mark_valid = true;
    s.peak_valid = true;

    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);

    CHECK(r.adapt);
    /* 0.5 * 7 = 3.5 dB. */
    CHECK_CLOSE(r.snr, 3.5f, 0.001f);
    /* 0.25 * 3.5 = 0.875 -> quantised to the 0.1 dB step of the cfg item. */
    CHECK_CLOSE(r.gist, 0.9f, 0.001f);
    /* A measured +20 means the tone is 20 Hz below the centre: the correction
     * is -20 Hz (HALLAZGOS 52). */
    CHECK_CLOSE(r.freq_offset_hz, -20.0f, 0.001f);
}

static void test_no_signal_does_not_adapt(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s = strong(0.0f);

    s.signal_ratio = 0.9f;      /* sum_signal/noise <= 1: noise, not a signal */
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);

    CHECK(!r.adapt);
    CHECK(!r.apply_snr && !r.apply_gist && !r.apply_peak_beta && !r.apply_noise_beta && !r.apply_freq);
    CHECK_CLOSE(r.snr, 0.0f, 0.0001f);    /* echo of "nothing applied yet" */

    /* Valid ratio but not enough margin: 4 dB is below the 6 dB gate. */
    s = strong(0.0f);
    s.mark_db = -76.0f;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK(!r.adapt);

    /* No mark at all in the window. */
    s = strong(0.0f);
    s.mark_valid = false;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK(!r.adapt);

    /* Exactly 1.0 is not > 1. */
    s = strong(0.0f);
    s.signal_ratio = 1.0f;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK(!r.adapt);
}

static void test_noise_beta_by_floor_stability(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s;

    /* sigma = sqrt(var). 0,5 dB -> 0,90 (cap of the stable band); 2 dB -> 0,80;
     * 5 dB -> 0,50; 8 dB stays at the nervous floor. */
    const float var[] = { 0.25f, 4.0f, 25.0f, 64.0f };
    const float want[] = { 0.90f, 0.80f, 0.50f, 0.50f };

    for (size_t i = 0; i < sizeof(var) / sizeof(var[0]); i++) {
        s = strong(0.0f);
        s.noise_var_db2 = var[i];
        s.peak_valid = false;
        cw_autotune_reset(&st);
        cw_autotune_compute(&st, &s, &r);
        CHECK(r.adapt);
        CHECK_CLOSE(r.noise_beta, want[i], 0.001f);
    }
}

static void test_peak_beta_clamps_and_short_block(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s = strong(0.0f);

    /* The real block (185.76 ms) drives the formula below the lower clamp for
     * every decodable WPM: that is the measured behaviour, not an accident. */
    const int wpm[] = { 5, 10, 20, 30, 40, 60 };
    for (size_t i = 0; i < sizeof(wpm) / sizeof(wpm[0]); i++) {
        s.wpm = wpm[i];
        cw_autotune_reset(&st);
        cw_autotune_compute(&st, &s, &r);
        CHECK_CLOSE(r.peak_beta, 0.10f, 0.001f);
    }

    /* The same formula on the (hypothetical) un-decimated block of 128/44100 s
     * shows the spread the issue describes: 0.95 at 10 WPM down to 0.71 at
     * 60 WPM. It pins that the function is not a constant. */
    s = strong(0.0f);
    s.block_dt_s = 128.0f / 44100.0f;
    s.wpm = 10;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.peak_beta, 0.95f, 0.001f);

    s.wpm = 40;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.peak_beta, 0.81f, 0.001f);

    /* Degenerate block: no divide by zero, the fastest setting. */
    s.block_dt_s = 0.0f;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.peak_beta, 0.10f, 0.001f);
}

static void test_snr_clamps(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s;

    /* 6.2 dB is the gate: 0.5 * 6.2 = 3.1 dB. */
    s = strong(0.0f);
    s.mark_db = -73.8f;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK(r.adapt);
    CHECK_CLOSE(r.snr, 3.1f, 0.001f);

    /* 60 dB of margin saturates the formula at 10, not at the MFK's 30. */
    s = strong(0.0f);
    s.noise_db = -100.0f;
    s.mark_db = -40.0f;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.snr, 10.0f, 0.001f);
}

/* --- deadband, averaging and the tone tracking ---------------------------- */

static void test_deadband_keeps_applied_value(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s = strong(0.0f);

    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.snr, 10.0f, 0.001f);

    /* 19.6 dB of margin -> 9.8 dB: 0.2 dB below the applied value, inside the
     * 2.7 dB deadband. Nothing is written. */
    s.mark_db = -60.4f;
    s.peak_valid = false;
    cw_autotune_compute(&st, &s, &r);
    CHECK(r.adapt);
    CHECK(!r.apply_snr);
    CHECK_CLOSE(r.snr, 10.0f, 0.001f);   /* the applied value, not the suggestion */

    /* 0.17 below on the betas is also inside the 0.085... no: 0.17 > 0.085.
     * With a stable floor the suggestion is still 0.90, so nothing changes. */
    CHECK(!r.apply_noise_beta);
}

static void test_qsb_jump_is_averaged(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s = strong(0.0f);

    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.snr, 10.0f, 0.001f);

    /* The next window drops to 8 dB of margin -> snr 4. The suggestion jumps
     * 6 dB (> 2 * 2.7), so it is averaged with the previous suggestion:
     * (10 + 4)/2 = 7. */
    s.mark_db = -72.0f;
    s.peak_valid = false;
    cw_autotune_compute(&st, &s, &r);
    CHECK(r.adapt);
    CHECK_CLOSE(r.snr, 7.0f, 0.001f);
    CHECK(r.apply_snr);                  /* 3 dB away, outside the deadband */

    /* A second identical window is now 3 dB below the previous suggestion
     * (inside the jump band), so it is applied as-is: 4. The average is a
     * one-shot brake, not a permanent low-pass. */
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.snr, 4.0f, 0.001f);
}

static void test_freq_sign_and_slew(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s = strong(100.0f);   /* measured +100 means "below" */

    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.freq_offset_hz, -30.0f, 0.001f);   /* correction = -peak */

    s.peak_freq_hz = -100.0f;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.freq_offset_hz, 30.0f, 0.001f);

    /* Bounded to +/-300 Hz and at most 30 Hz per window: ten windows of a
     * 1000 Hz error stop exactly at the bound. */
    s.peak_freq_hz = 1000.0f;
    cw_autotune_reset(&st);
    for (int i = 0; i < 10; i++) {
        cw_autotune_compute(&st, &s, &r);
        CHECK(r.apply_freq);
    }
    CHECK_CLOSE(r.freq_offset_hz, -300.0f, 0.001f);

    /* Inside the deadband (10 Hz) nothing moves. */
    s = strong(0.0f);
    s.peak_freq_hz = -5.0f;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK(!r.apply_freq);
    CHECK_CLOSE(r.freq_offset_hz, 0.0f, 0.001f);
}

static void test_invalid_peak_keeps_the_tone(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s = strong(100.0f);

    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK_CLOSE(r.freq_offset_hz, -30.0f, 0.001f);

    /* No spectral peak in this window: the tone stays where it was. */
    s.peak_valid = false;
    cw_autotune_compute(&st, &s, &r);
    CHECK(!r.apply_freq);
    CHECK_CLOSE(r.freq_offset_hz, -30.0f, 0.001f);
}

static void test_nan_is_rejected(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s = strong(0.0f);

    s.mark_db = NAN;
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK(!r.adapt);
}

static void test_reset_reapplies(void) {
    cw_autotune_state_t st;
    cw_autotune_result_t r;
    cw_autotune_stats_t s = strong(0.0f);

    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);

    /* Same window again: deadband holds everything. */
    cw_autotune_compute(&st, &s, &r);
    CHECK(!r.apply_snr && !r.apply_peak_beta);

    /* AUTO toggled off and on: the next window calibrates from scratch. */
    cw_autotune_reset(&st);
    cw_autotune_compute(&st, &s, &r);
    CHECK(r.adapt);
    CHECK(r.apply_snr && r.apply_noise_beta);
}

static void test_null_arguments_are_safe(void) {
    cw_autotune_result_t r;

    cw_autotune_reset(NULL);
    memset(&r, 0xA5, sizeof(r));
    cw_autotune_compute(NULL, NULL, &r);
    CHECK(!r.adapt);
    CHECK(!r.apply_snr);
    cw_autotune_compute(NULL, NULL, NULL);
}

void test_cw_autotune(void) {
    test_tau_peak_by_wpm();
    test_percentile_nearest_rank();
    test_mean_and_variance();
    test_strong_signal();
    test_weak_signal();
    test_no_signal_does_not_adapt();
    test_noise_beta_by_floor_stability();
    test_peak_beta_clamps_and_short_block();
    test_snr_clamps();
    test_deadband_keeps_applied_value();
    test_qsb_jump_is_averaged();
    test_freq_sign_and_slew();
    test_invalid_peak_keeps_the_tone();
    test_nan_is_rejected();
    test_reset_reapplies();
    test_null_arguments_are_safe();
}

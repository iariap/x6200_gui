/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Tests host del router RX live (#364): ring, estados, prioridad TX/audio
 *  generado, staleness y fail-closed de MUX. Sin PulseAudio ni hardware.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "test_framework.h"
#include "rx_audio_live.h"

typedef struct {
    int open_calls;
    int close_calls;
    int ready_calls;
    int write_calls;
    int mux_true;
    int mux_false;
    int fail_open;
    int fail_mux;
    int fail_write;
    int writes_while_fail;
    int ready;
    int mux_ready;
    size_t last_count;
    int16_t last_first;
} fake_t;

static bool fake_open(void *ctx) {
    fake_t *f = ctx;
    f->open_calls++;
    return !f->fail_open;
}

static void fake_close(void *ctx) {
    fake_t *f = ctx;
    f->close_calls++;
}

static bool fake_ready(void *ctx) {
    fake_t *f = ctx;
    f->ready_calls++;
    return f->ready != 0;
}

static bool fake_write(void *ctx, const int16_t *samples, size_t count) {
    fake_t *f = ctx;
    f->write_calls++;
    f->last_count = count;
    f->last_first = count ? samples[0] : 0;
    if (f->fail_write) {
        f->writes_while_fail++;
        return false;
    }
    return true;
}

static bool fake_mux_ready(void *ctx) {
    fake_t *f = ctx;
    return f->mux_ready != 0;
}

static int fake_mux(void *ctx, bool dac) {
    fake_t *f = ctx;
    if (dac) {
        f->mux_true++;
        return f->fail_mux ? -1 : 0;
    }
    f->mux_false++;
    return 0;
}

static bool wait_state(rx_audio_live_state_t want, int timeout_ms) {
    for (int i = 0; i < timeout_ms; i++) {
        if (rx_audio_live_get_state() == want) {
            return true;
        }
        usleep(1000);
    }
    return false;
}

static void submit_blocks(int blocks, int16_t seed) {
    int16_t buf[RX_AUDIO_LIVE_BLOCK_SAMPLES];

    for (int b = 0; b < blocks; b++) {
        for (size_t i = 0; i < RX_AUDIO_LIVE_BLOCK_SAMPLES; i++) {
            buf[i] = (int16_t) (seed + b + (int) i);
        }
        rx_audio_live_submit(buf, RX_AUDIO_LIVE_BLOCK_SAMPLES);
    }
}

static void setup(fake_t *f, bool fail_mux, bool fail_open) {
    memset(f, 0, sizeof(*f));
    f->ready = 1;
    f->mux_ready = 1;
    f->fail_mux = fail_mux;
    f->fail_open = fail_open;
    rx_audio_live_backend_t be = {
        .ctx = f,
        .open = fake_open,
        .close = fake_close,
        .ready = fake_ready,
        .write = fake_write,
        .mux_ready = fake_mux_ready,
        .mux_request = fake_mux,
    };
    rx_audio_live_init(&be);
}

static void teardown(void) {
    rx_audio_live_set_enabled(false);
    usleep(30000);
    rx_audio_live_shutdown();
}

static void test_disabled_is_inert(void) {
    fake_t f;
    setup(&f, false, false);
    submit_blocks(4, 1);
    usleep(30000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.open_calls == 0);
    CHECK(f.write_calls == 0);
    CHECK(f.mux_true == 0);
    CHECK(rx_audio_live_pending_samples() == 0);
    teardown();
}


static void test_mux_gate_false_blocks_dac(void) {
    fake_t f;
    setup(&f, false, false);
    f.mux_ready = 0;   /* gate A/B/C no acreditado */
    rx_audio_live_set_enabled(true);
    submit_blocks(2, 1);
    usleep(150000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.mux_true == 0);
    CHECK(f.write_calls == 0);
    teardown();
}

static void test_rx_dac_and_restore(void) {
    fake_t f;
    setup(&f, false, false);
    rx_audio_live_set_enabled(true);
    submit_blocks(2, 10);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));
    CHECK(f.open_calls >= 1);
    CHECK(f.mux_true == 1);

    submit_blocks(1, 20);
    usleep(50000);
    CHECK(f.write_calls >= 1);
    CHECK(f.last_count == RX_AUDIO_LIVE_BLOCK_SAMPLES);

    rx_audio_live_set_enabled(false);
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    CHECK(f.mux_false >= 1);
    CHECK(f.close_calls >= 1);
    teardown();
}

static void test_mux_fail_keeps_base(void) {
    fake_t f;
    setup(&f, true, false);
    rx_audio_live_set_enabled(true);
    submit_blocks(2, 1);
    usleep(150000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.mux_true >= 1);
    CHECK(f.write_calls == 0);
    teardown();
}

static void test_tx_suspends(void) {
    fake_t f;
    setup(&f, false, false);
    rx_audio_live_set_enabled(true);
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));

    rx_audio_live_set_tx(true);
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    CHECK(f.mux_false >= 1);
    int writes_before = f.write_calls;
    submit_blocks(2, 2);
    usleep(50000);
    CHECK(f.write_calls == writes_before);
    rx_audio_live_set_tx(false);
    teardown();
}

static void test_generated_suspends(void) {
    fake_t f;
    setup(&f, false, false);
    rx_audio_live_set_enabled(true);
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));

    rx_audio_live_set_generated_audio(true);
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    CHECK(f.mux_false >= 1);
    rx_audio_live_set_generated_audio(false);
    teardown();
}

static void test_stale_returns_base(void) {
    fake_t f;
    setup(&f, false, false);
    rx_audio_live_set_enabled(true);
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));

    usleep(RX_AUDIO_LIVE_STALE_MS * 1000 + 80000);
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    CHECK(f.mux_false >= 1);
    rx_audio_live_stats_t st;
    rx_audio_live_get_stats(&st);
    CHECK(st.last_suspend == RX_AUDIO_LIVE_SUSPEND_STALE);
    teardown();
}

static void test_capture_error_suspends(void) {
    fake_t f;
    setup(&f, false, false);
    rx_audio_live_set_enabled(true);
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));

    rx_audio_live_notify_capture_error();
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    rx_audio_live_stats_t st;
    rx_audio_live_get_stats(&st);
    CHECK(st.last_suspend == RX_AUDIO_LIVE_SUSPEND_CAPTURE);
    teardown();
}

static void test_ring_overflow_drops_oldest(void) {
    fake_t f;
    setup(&f, false, true);   /* open fail: el worker no consume el ring */
    rx_audio_live_set_enabled(true);
    submit_blocks(8, 1);
    usleep(30000);
    CHECK(rx_audio_live_pending_samples() <= RX_AUDIO_LIVE_BLOCKS * RX_AUDIO_LIVE_BLOCK_SAMPLES);
    rx_audio_live_stats_t st;
    rx_audio_live_get_stats(&st);
    CHECK(st.dropped_blocks > 0);
    teardown();
}

static void test_shutdown_restores(void) {
    fake_t f;
    setup(&f, false, false);
    rx_audio_live_set_enabled(true);
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));
    rx_audio_live_shutdown();
    CHECK(f.mux_false >= 1);
    CHECK(f.close_calls >= 1);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
}

void test_rx_audio_live(void) {
    test_disabled_is_inert();
    test_mux_gate_false_blocks_dac();
    test_rx_dac_and_restore();
    test_mux_fail_keeps_base();
    test_tx_suspends();
    test_generated_suspends();
    test_stale_returns_base();
    test_capture_error_suspends();
    test_ring_overflow_drops_oldest();
    test_shutdown_restores();
}

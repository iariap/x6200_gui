/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Tests host del router RX live (#364): ring, estados, prioridad TX/audio
 *  generado, staleness, backpressure y obligacion de restaurar BASE.
 *  Sin PulseAudio ni hardware.
 */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "test_framework.h"
#include "rx_audio_live.h"

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  cv;

    int open_calls;
    int close_calls;
    int ready_calls;
    int write_calls;
    int mux_true;
    int mux_false;

    int fail_open;
    int ready;
    int mux_ready;
    int write_result;
    int mux_on_result;
    int mux_off_result;

    int block_mux_on;
    int mux_entered;
    int mux_release;

    int last_count;
    int16_t last_first;
    int16_t last_block[RX_AUDIO_LIVE_BLOCK_SAMPLES];
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

static int fake_write(void *ctx, const int16_t *samples, size_t count) {
    fake_t *f = ctx;
    f->write_calls++;
    f->last_count = (int) count;
    f->last_first = count ? samples[0] : 0;
    if (count == RX_AUDIO_LIVE_BLOCK_SAMPLES) {
        memcpy(f->last_block, samples, sizeof(f->last_block));
    }
    return f->write_result;
}

static bool fake_mux_ready(void *ctx) {
    fake_t *f = ctx;
    return f->mux_ready != 0;
}

static int fake_mux(void *ctx, bool dac) {
    fake_t *f = ctx;

    if (!dac) {
        f->mux_false++;
        return f->mux_off_result;
    }

    f->mux_true++;

    pthread_mutex_lock(&f->lock);
    if (f->block_mux_on) {
        f->mux_entered = 1;
        pthread_cond_broadcast(&f->cv);
        while (!f->mux_release) {
            pthread_cond_wait(&f->cv, &f->lock);
        }
    }
    pthread_mutex_unlock(&f->lock);

    return f->mux_on_result;
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

static bool wait_mux_true(fake_t *f, int timeout_ms) {
    for (int i = 0; i < timeout_ms; i++) {
        if (f->mux_true > 0) {
            return true;
        }
        usleep(1000);
    }
    return false;
}

static bool wait_fake_entered(fake_t *f, int timeout_ms) {
    bool entered = false;

    for (int i = 0; i < timeout_ms; i++) {
        pthread_mutex_lock(&f->lock);
        entered = f->mux_entered != 0;
        pthread_mutex_unlock(&f->lock);
        if (entered) {
            return true;
        }
        usleep(1000);
    }
    return false;
}

static void release_mux(fake_t *f) {
    pthread_mutex_lock(&f->lock);
    f->mux_release = 1;
    pthread_cond_broadcast(&f->cv);
    pthread_mutex_unlock(&f->lock);
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

static void setup(fake_t *f) {
    memset(f, 0, sizeof(*f));
    pthread_mutex_init(&f->lock, NULL);
    pthread_cond_init(&f->cv, NULL);
    f->ready = 1;
    f->mux_ready = 1;
    f->write_result = RX_AUDIO_LIVE_WRITE_OK;
    f->mux_on_result = RX_AUDIO_LIVE_MUX_OK;
    f->mux_off_result = RX_AUDIO_LIVE_MUX_OK;

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

static void teardown(fake_t *f) {
    /* Si un test dejo una restauracion fallida, se acepta el estado de error;
     * shutdown no debe crashear ni reactivar DAC. */
    (void) rx_audio_live_set_enabled(false);
    usleep(30000);
    rx_audio_live_shutdown();
    pthread_mutex_destroy(&f->lock);
    pthread_cond_destroy(&f->cv);
}

static void test_disabled_is_inert(void) {
    fake_t f;
    setup(&f);
    submit_blocks(4, 1);
    usleep(30000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.open_calls == 0);
    CHECK(f.write_calls == 0);
    CHECK(f.mux_true == 0);
    CHECK(rx_audio_live_pending_samples() == 0);
    teardown(&f);
}

static void test_mux_gate_false_blocks_dac(void) {
    fake_t f;
    setup(&f);
    f.mux_ready = 0;
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    usleep(150000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.mux_true == 0);
    CHECK(f.write_calls == 0);
    teardown(&f);
}

static void test_rx_dac_and_restore(void) {
    fake_t f;
    setup(&f);
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 10);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));
    CHECK(f.open_calls >= 1);
    CHECK(f.mux_true == 1);

    submit_blocks(1, 20);
    usleep(50000);
    CHECK(f.write_calls >= 1);
    CHECK(f.last_count == RX_AUDIO_LIVE_BLOCK_SAMPLES);

    CHECK(rx_audio_live_set_enabled(false));
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    CHECK(f.mux_false >= 1);
    CHECK(f.close_calls >= 1);
    teardown(&f);
}

static void test_mux_not_entered_keeps_base(void) {
    fake_t f;
    setup(&f);
    f.mux_on_result = RX_AUDIO_LIVE_MUX_NOT_ENTERED;

    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    usleep(150000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.mux_true >= 1);
    CHECK(f.mux_false == 0);   /* no hubo transicion que restaurar */
    CHECK(f.write_calls == 0);
    teardown(&f);
}

static void test_mux_unknown_forces_restore(void) {
    fake_t f;
    setup(&f);
    f.mux_on_result = RX_AUDIO_LIVE_MUX_UNKNOWN;

    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_mux_true(&f, 1000));
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    CHECK(f.mux_false >= 1);
    CHECK(!rx_audio_live_restore_unconfirmed());
    CHECK(f.write_calls == 0);
    teardown(&f);
}

static void test_restore_failure_latches_error(void) {
    fake_t f;
    setup(&f);
    f.mux_off_result = RX_AUDIO_LIVE_MUX_UNKNOWN;

    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));
    CHECK(rx_audio_live_set_enabled(false));
    CHECK(wait_state(RX_AUDIO_LIVE_BASE_RESTORE_UNCONFIRMED, 1000));
    CHECK(rx_audio_live_restore_unconfirmed());
    CHECK(!rx_audio_live_set_enabled(true));
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE_RESTORE_UNCONFIRMED);
    CHECK(f.mux_false >= 1);
    teardown(&f);
}

static void test_disable_while_mux_on_restores(void) {
    fake_t f;
    setup(&f);
    f.block_mux_on = 1;

    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_fake_entered(&f, 1000));

    /* Deshabilitar mientras la seleccion DAC esta en curso. */
    CHECK(rx_audio_live_set_enabled(false));
    release_mux(&f);

    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1500));
    CHECK(f.mux_true == 1);
    CHECK(f.mux_false >= 1);
    CHECK(f.close_calls >= 1);
    CHECK(!rx_audio_live_restore_unconfirmed());
    teardown(&f);
}

static void test_tx_during_mux_on_restores(void) {
    fake_t f;
    setup(&f);
    f.block_mux_on = 1;

    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_fake_entered(&f, 1000));
    rx_audio_live_set_tx(true);
    release_mux(&f);

    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1500));
    CHECK(f.mux_false >= 1);
    teardown(&f);
}


static void test_pcm_passthrough(void) {
    fake_t f;
    setup(&f);
    int16_t buf[RX_AUDIO_LIVE_BLOCK_SAMPLES];

    for (size_t i = 0; i < RX_AUDIO_LIVE_BLOCK_SAMPLES; i++) {
        buf[i] = (int16_t) (1000 + (int) i);
    }

    CHECK(rx_audio_live_set_enabled(true));
    rx_audio_live_submit(buf, RX_AUDIO_LIVE_BLOCK_SAMPLES);
    rx_audio_live_submit(buf, RX_AUDIO_LIVE_BLOCK_SAMPLES);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));
    for (int i = 0; i < 200 && f.write_calls == 0; i++) {
        usleep(1000);
    }
    CHECK(f.write_calls >= 1);
    CHECK(f.last_block[0] == buf[0]);
    CHECK(f.last_block[RX_AUDIO_LIVE_BLOCK_SAMPLES - 1] ==
          buf[RX_AUDIO_LIVE_BLOCK_SAMPLES - 1]);
    teardown(&f);
}

static void test_tx_before_enable_blocks_dac(void) {
    fake_t f;
    setup(&f);
    rx_audio_live_set_tx(true);
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    usleep(150000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.mux_true == 0);
    rx_audio_live_set_tx(false);
    teardown(&f);
}

static void test_generated_before_enable_blocks_dac(void) {
    fake_t f;
    setup(&f);
    rx_audio_live_set_generated_audio(true);
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    usleep(150000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.mux_true == 0);
    rx_audio_live_set_generated_audio(false);
    teardown(&f);
}

static void test_tx_suspends(void) {
    fake_t f;
    setup(&f);
    CHECK(rx_audio_live_set_enabled(true));
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
    teardown(&f);
}

static void test_generated_suspends(void) {
    fake_t f;
    setup(&f);
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));

    rx_audio_live_set_generated_audio(true);
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    CHECK(f.mux_false >= 1);
    rx_audio_live_set_generated_audio(false);
    teardown(&f);
}

static void test_stale_returns_base(void) {
    fake_t f;
    setup(&f);
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));

    usleep(RX_AUDIO_LIVE_STALE_MS * 1000 + 80000);
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    CHECK(f.mux_false >= 1);
    rx_audio_live_stats_t st;
    rx_audio_live_get_stats(&st);
    CHECK(st.last_suspend == RX_AUDIO_LIVE_SUSPEND_STALE);
    teardown(&f);
}

static void test_capture_error_suspends(void) {
    fake_t f;
    setup(&f);
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));

    rx_audio_live_notify_capture_error();
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    rx_audio_live_stats_t st;
    rx_audio_live_get_stats(&st);
    CHECK(st.last_suspend == RX_AUDIO_LIVE_SUSPEND_CAPTURE);
    teardown(&f);
}

static void test_ring_overflow_drops_oldest(void) {
    fake_t f;
    setup(&f);
    f.fail_open = 1;   /* el worker no consume el ring */
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(8, 1);
    usleep(30000);
    CHECK(rx_audio_live_pending_samples() <=
          RX_AUDIO_LIVE_BLOCKS * RX_AUDIO_LIVE_BLOCK_SAMPLES);
    rx_audio_live_stats_t st;
    rx_audio_live_get_stats(&st);
    CHECK(st.dropped_blocks > 0);
    teardown(&f);
}

static void test_backpressure_degrades(void) {
    fake_t f;
    setup(&f);
    f.write_result = RX_AUDIO_LIVE_WRITE_NO_SPACE;

    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));
    for (int i = 0; i < 10 && rx_audio_live_get_state() == RX_AUDIO_LIVE_RX_DAC; i++) {
        submit_blocks(1, (int16_t) (100 + i));
        usleep(30000);
    }
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1500));
    rx_audio_live_stats_t st;
    rx_audio_live_get_stats(&st);
    CHECK(st.backpressure >= 1);
    CHECK(f.mux_false >= 1);
    teardown(&f);
}

static void test_write_error_degrades(void) {
    fake_t f;
    setup(&f);
    f.write_result = RX_AUDIO_LIVE_WRITE_ERROR;

    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_mux_true(&f, 1000));
    CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
    CHECK(f.mux_false >= 1);
    teardown(&f);
}

static void test_open_ready_fail_no_dac(void) {
    fake_t f;
    setup(&f);
    f.fail_open = 1;
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    usleep(200000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.mux_true == 0);
    teardown(&f);
}

static void test_ready_fail_no_dac(void) {
    fake_t f;
    setup(&f);
    f.ready = 0;
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    usleep(200000);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    CHECK(f.mux_true == 0);
    teardown(&f);
}

static void test_can_activate_gate(void) {
    fake_t f;
    setup(&f);
    CHECK(rx_audio_live_can_activate());
    f.mux_ready = 0;
    CHECK(!rx_audio_live_can_activate());
    teardown(&f);
}

static void test_repeated_lifecycle(void) {
    for (int i = 0; i < 5; i++) {
        fake_t f;
        setup(&f);
        CHECK(rx_audio_live_set_enabled(true));
        submit_blocks(2, (int16_t) i);
        CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));
        CHECK(rx_audio_live_set_enabled(false));
        CHECK(wait_state(RX_AUDIO_LIVE_BASE, 1000));
        teardown(&f);
    }
}

static void test_shutdown_restores(void) {
    fake_t f;
    setup(&f);
    CHECK(rx_audio_live_set_enabled(true));
    submit_blocks(2, 1);
    CHECK(wait_state(RX_AUDIO_LIVE_RX_DAC, 1000));
    rx_audio_live_shutdown();
    CHECK(f.mux_false >= 1);
    CHECK(f.close_calls >= 1);
    CHECK(rx_audio_live_get_state() == RX_AUDIO_LIVE_BASE);
    pthread_mutex_destroy(&f.lock);
    pthread_cond_destroy(&f.cv);
}

void test_rx_audio_live(void) {
    test_disabled_is_inert();
    test_mux_gate_false_blocks_dac();
    test_rx_dac_and_restore();
    test_mux_not_entered_keeps_base();
    test_mux_unknown_forces_restore();
    test_restore_failure_latches_error();
    test_disable_while_mux_on_restores();
    test_tx_during_mux_on_restores();
    test_tx_suspends();
    test_generated_suspends();
    test_stale_returns_base();
    test_capture_error_suspends();
    test_ring_overflow_drops_oldest();
    test_pcm_passthrough();
    test_tx_before_enable_blocks_dac();
    test_generated_before_enable_blocks_dac();
    test_backpressure_degrades();
    test_write_error_degrades();
    test_open_ready_fail_no_dac();
    test_ready_fail_no_dac();
    test_can_activate_gate();
    test_repeated_lifecycle();
    test_shutdown_restores();
}

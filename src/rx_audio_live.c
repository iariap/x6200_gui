/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Router RX procesado -> DAC (issue #364 / Epic #362). Ver rx_audio_live.h.
 */

#define _POSIX_C_SOURCE 200809L

#include "rx_audio_live.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#define NBLOCKS RX_AUDIO_LIVE_BLOCKS
#define NSAMP   RX_AUDIO_LIVE_BLOCK_SAMPLES

typedef enum {
    ACT_NONE = 0,
    ACT_OPEN,
    ACT_READY,
    ACT_WRITE,
    ACT_MUX_ON,
    ACT_CLEANUP,
} action_t;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;
static pthread_t       worker;
static bool            initialized;
static bool            running;
static bool            stop_worker;

static rx_audio_live_backend_t be;

static bool            enabled;
static bool            tx_active;
static bool            generated_active;
static bool            capture_fault;
static bool            sink_open;
static bool            sink_ready;
static bool            dac_selected;
static rx_audio_live_state_t state = RX_AUDIO_LIVE_BASE;

static int16_t ring[NBLOCKS][NSAMP];
static unsigned ring_head;      /* bloque mas viejo */
static unsigned ring_count;
static int16_t staging[NSAMP];
static size_t  staging_count;
static uint64_t last_submit_ms;
static uint64_t retry_after_ms;

static rx_audio_live_stats_t stats;

static uint64_t now_ms(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t) ts.tv_sec * 1000ULL + (uint64_t) ts.tv_nsec / 1000000ULL;
}

static bool stale_locked(uint64_t now) {
    return last_submit_ms == 0 || now < last_submit_ms ||
           now - last_submit_ms > RX_AUDIO_LIVE_STALE_MS;
}

static void clear_buffers_locked(void) {
    ring_head = 0;
    ring_count = 0;
    staging_count = 0;
    memset(staging, 0, sizeof(staging));
}

static void push_block_locked(const int16_t *block) {
    unsigned dst;

    if (ring_count == NBLOCKS) {
        /* Overflow: se descarta el bloque mas viejo (KISS, nunca crece sin tope). */
        ring_head = (ring_head + 1u) % NBLOCKS;
        ring_count--;
        stats.dropped_blocks++;
    }

    dst = (ring_head + ring_count) % NBLOCKS;
    memcpy(ring[dst], block, sizeof(ring[dst]));
    ring_count++;
}

static bool pop_block_locked(int16_t *out) {
    if (ring_count == 0) {
        return false;
    }

    memcpy(out, ring[ring_head], sizeof(ring[ring_head]));
    ring_head = (ring_head + 1u) % NBLOCKS;
    ring_count--;
    return true;
}

static bool suspend_reason_locked(rx_audio_live_suspend_t *why) {
    if (!enabled) {
        *why = RX_AUDIO_LIVE_SUSPEND_SHUTDOWN;
        return true;
    }
    if (tx_active) {
        *why = RX_AUDIO_LIVE_SUSPEND_TX;
        return true;
    }
    if (generated_active) {
        *why = RX_AUDIO_LIVE_SUSPEND_GENERATED;
        return true;
    }
    if (capture_fault) {
        *why = RX_AUDIO_LIVE_SUSPEND_CAPTURE;
        return true;
    }
    if (state == RX_AUDIO_LIVE_RX_DAC && stale_locked(now_ms())) {
        *why = RX_AUDIO_LIVE_SUSPEND_STALE;
        return true;
    }
    return false;
}

/* Corre fuera del lock: el callback de captura no espera estas llamadas. */
static void do_cleanup(const rx_audio_live_suspend_t why) {
    bool had_dac;

    pthread_mutex_lock(&lock);
    had_dac = dac_selected;
    pthread_mutex_unlock(&lock);

    if (had_dac && be.mux_request != NULL) {
        (void) be.mux_request(be.ctx, false);
    }
    if (sink_open && be.close != NULL) {
        be.close(be.ctx);
    }

    pthread_mutex_lock(&lock);
    stats.base_requests += had_dac ? 1u : 0u;
    stats.closes += sink_open ? 1u : 0u;
    stats.last_suspend = why;
    sink_open = false;
    sink_ready = false;
    dac_selected = false;
    state = RX_AUDIO_LIVE_BASE;
    clear_buffers_locked();
    pthread_mutex_unlock(&lock);
}

static bool open_stream(void) {
    bool ok = (be.open != NULL) && be.open(be.ctx);

    pthread_mutex_lock(&lock);
    sink_open = ok;
    if (ok) {
        stats.opens++;
    }
    pthread_mutex_unlock(&lock);
    return ok;
}

static void worker_loop(void);

static void *worker_main(void *unused) {
    (void) unused;
    worker_loop();
    return NULL;
}

static void worker_loop(void) {
    while (true) {
        action_t act = ACT_NONE;
        rx_audio_live_suspend_t why = RX_AUDIO_LIVE_SUSPEND_NONE;
        int16_t block[NSAMP];
        bool have_block = false;
        uint64_t now;

        pthread_mutex_lock(&lock);
        if (stop_worker) {
            pthread_mutex_unlock(&lock);
            break;
        }

        now = now_ms();

        if (suspend_reason_locked(&why)) {
            bool cleanup = (state != RX_AUDIO_LIVE_BASE) || sink_open || dac_selected;
            if (!cleanup) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_nsec += RX_AUDIO_LIVE_WORKER_WAIT_MS * 1000000L;
                if (ts.tv_nsec >= 1000000000L) {
                    ts.tv_sec++;
                    ts.tv_nsec -= 1000000000L;
                }
                (void) pthread_cond_timedwait(&cv, &lock, &ts);
                pthread_mutex_unlock(&lock);
                continue;
            }
            pthread_mutex_unlock(&lock);
            do_cleanup(why);
            continue;
        }

        if (retry_after_ms != 0 && now < retry_after_ms) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += RX_AUDIO_LIVE_WORKER_WAIT_MS * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }
            (void) pthread_cond_timedwait(&cv, &lock, &ts);
            pthread_mutex_unlock(&lock);
            continue;
        }
        retry_after_ms = 0;

        if (!sink_open) {
            act = ACT_OPEN;
        } else if (!sink_ready) {
            act = ACT_READY;
        } else if (state == RX_AUDIO_LIVE_BASE) {
            bool mux_ready = (be.mux_ready != NULL) && be.mux_ready(be.ctx);
            if (mux_ready && ring_count >= RX_AUDIO_LIVE_START_BLOCKS && !stale_locked(now)) {
                state = RX_AUDIO_LIVE_PREPARE_DAC;
                act = ACT_MUX_ON;
            }
        } else if (state == RX_AUDIO_LIVE_PREPARE_DAC) {
            /* Sin accion del worker: espera el resultado incremental (no se llega
             * aca porque ACT_MUX_ON se resuelve en la misma vuelta). */
        } else if (state == RX_AUDIO_LIVE_RX_DAC) {
            if (pop_block_locked(block)) {
                have_block = true;
                act = ACT_WRITE;
            } else if (sink_ready && !stale_locked(now)) {
                /* Sin bloque disponible: no se inventa audio. Se espera el proximo submit. */
                stats.underruns++;
            }
        } else if (state == RX_AUDIO_LIVE_DEGRADED_BASE) {
            pthread_mutex_unlock(&lock);
            do_cleanup(RX_AUDIO_LIVE_SUSPEND_SINK);
            continue;
        }

        if (act == ACT_NONE) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += RX_AUDIO_LIVE_WORKER_WAIT_MS * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }
            (void) pthread_cond_timedwait(&cv, &lock, &ts);
            pthread_mutex_unlock(&lock);
            continue;
        }
        pthread_mutex_unlock(&lock);

        switch (act) {
            case ACT_OPEN:
                if (!open_stream()) {
                    pthread_mutex_lock(&lock);
                    retry_after_ms = now_ms() + 100u;
                    pthread_mutex_unlock(&lock);
                }
                break;

            case ACT_READY: {
                bool rdy = (be.ready != NULL) && be.ready(be.ctx);
                pthread_mutex_lock(&lock);
                sink_ready = rdy;
                if (!rdy) {
                    retry_after_ms = now_ms() + 20u;
                }
                pthread_mutex_unlock(&lock);
                break;
            }

            case ACT_WRITE: {
                bool ok = (be.write != NULL) && be.write(be.ctx, block, NSAMP);
                pthread_mutex_lock(&lock);
                if (ok) {
                    stats.writes++;
                } else {
                    stats.underruns++;
                    state = RX_AUDIO_LIVE_DEGRADED_BASE;
                }
                pthread_mutex_unlock(&lock);
                break;
            }

            case ACT_MUX_ON: {
                int r = (be.mux_request != NULL) ? be.mux_request(be.ctx, true) : -1;
                pthread_mutex_lock(&lock);
                stats.dac_requests++;
                if (r == 0 && enabled && !tx_active && !generated_active && !capture_fault) {
                    dac_selected = true;
                    state = RX_AUDIO_LIVE_RX_DAC;
                } else {
                    stats.last_suspend = RX_AUDIO_LIVE_SUSPEND_MUX;
                    state = RX_AUDIO_LIVE_DEGRADED_BASE;
                }
                pthread_mutex_unlock(&lock);
                break;
            }

            case ACT_CLEANUP:
            case ACT_NONE:
                break;
        }
        (void) have_block;
    }
}

void rx_audio_live_init(const rx_audio_live_backend_t *backend) {
    pthread_mutex_lock(&lock);
    if (backend != NULL) {
        be = *backend;
    } else {
        memset(&be, 0, sizeof(be));
    }
    if (!initialized) {
        initialized = true;
        stop_worker = false;
        enabled = false;
        tx_active = false;
        generated_active = false;
        capture_fault = false;
        sink_open = false;
        sink_ready = false;
        dac_selected = false;
        state = RX_AUDIO_LIVE_BASE;
        last_submit_ms = 0;
        memset(&stats, 0, sizeof(stats));
        clear_buffers_locked();
        if (pthread_create(&worker, NULL, worker_main, NULL) == 0) {
            running = true;
        } else {
            running = false;
            initialized = false;
        }
    }
    pthread_mutex_unlock(&lock);
}

void rx_audio_live_shutdown(void) {
    pthread_mutex_lock(&lock);
    if (!initialized) {
        pthread_mutex_unlock(&lock);
        return;
    }
    stop_worker = true;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&lock);

    if (running) {
        (void) pthread_join(worker, NULL);
        running = false;
    }

    /* El worker ya no corre: cerrar el stream y pedir BASE/restore sin esperar
     * a que otro hilo lo haga. Es el unico punto fuera del worker que toca el
     * backend, y solo despues del join. */
    do_cleanup(RX_AUDIO_LIVE_SUSPEND_SHUTDOWN);

    pthread_mutex_lock(&lock);
    initialized = false;
    stop_worker = false;
    enabled = false;
    tx_active = false;
    generated_active = false;
    capture_fault = false;
    sink_open = false;
    sink_ready = false;
    dac_selected = false;
    state = RX_AUDIO_LIVE_BASE;
    clear_buffers_locked();
    pthread_mutex_unlock(&lock);
}

void rx_audio_live_set_enabled(bool on) {
    pthread_mutex_lock(&lock);
    enabled = on;
    if (!on) {
        clear_buffers_locked();
    }
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&lock);
}

bool rx_audio_live_enabled(void) {
    bool v;
    pthread_mutex_lock(&lock);
    v = enabled;
    pthread_mutex_unlock(&lock);
    return v;
}

void rx_audio_live_submit(const int16_t *samples, size_t count) {
    size_t done = 0;

    if (samples == NULL || count == 0) {
        return;
    }

    pthread_mutex_lock(&lock);
    if (!enabled || tx_active || generated_active) {
        pthread_mutex_unlock(&lock);
        return;
    }

    capture_fault = false;
    last_submit_ms = now_ms();

    while (done < count) {
        size_t room = NSAMP - staging_count;
        size_t chunk = count - done;

        if (chunk > room) {
            chunk = room;
        }
        memcpy(&staging[staging_count], &samples[done], chunk * sizeof(int16_t));
        staging_count += chunk;
        done += chunk;

        if (staging_count == NSAMP) {
            push_block_locked(staging);
            staging_count = 0;
        }
    }
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&lock);
}

void rx_audio_live_set_tx(bool active) {
    pthread_mutex_lock(&lock);
    tx_active = active;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&lock);
}

void rx_audio_live_set_generated_audio(bool active) {
    pthread_mutex_lock(&lock);
    generated_active = active;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&lock);
}

void rx_audio_live_notify_capture_error(void) {
    pthread_mutex_lock(&lock);
    capture_fault = true;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&lock);
}

rx_audio_live_state_t rx_audio_live_get_state(void) {
    rx_audio_live_state_t v;
    pthread_mutex_lock(&lock);
    v = state;
    pthread_mutex_unlock(&lock);
    return v;
}

const char *rx_audio_live_state_name(rx_audio_live_state_t s) {
    switch (s) {
        case RX_AUDIO_LIVE_BASE: return "BASE";
        case RX_AUDIO_LIVE_PREPARE_DAC: return "PREPARE_DAC";
        case RX_AUDIO_LIVE_RX_DAC: return "RX_DAC";
        case RX_AUDIO_LIVE_DEGRADED_BASE: return "DEGRADED_BASE";
        default: return "?";
    }
}

const char *rx_audio_live_suspend_name(rx_audio_live_suspend_t s) {
    switch (s) {
        case RX_AUDIO_LIVE_SUSPEND_NONE: return "none";
        case RX_AUDIO_LIVE_SUSPEND_TX: return "tx";
        case RX_AUDIO_LIVE_SUSPEND_GENERATED: return "generated";
        case RX_AUDIO_LIVE_SUSPEND_CAPTURE: return "capture";
        case RX_AUDIO_LIVE_SUSPEND_STALE: return "stale";
        case RX_AUDIO_LIVE_SUSPEND_SINK: return "sink";
        case RX_AUDIO_LIVE_SUSPEND_MUX: return "mux";
        case RX_AUDIO_LIVE_SUSPEND_SHUTDOWN: return "shutdown";
        default: return "?";
    }
}

void rx_audio_live_get_stats(rx_audio_live_stats_t *out) {
    if (out == NULL) {
        return;
    }
    pthread_mutex_lock(&lock);
    *out = stats;
    pthread_mutex_unlock(&lock);
}

size_t rx_audio_live_pending_samples(void) {
    size_t n;
    pthread_mutex_lock(&lock);
    n = staging_count + (size_t) ring_count * NSAMP;
    pthread_mutex_unlock(&lock);
    return n;
}

bool rx_audio_live_can_activate(void) {
    bool ok;
    pthread_mutex_lock(&lock);
    ok = initialized && be.open != NULL && be.write != NULL &&
         be.mux_ready != NULL && be.mux_ready(be.ctx) &&
         be.mux_request != NULL;
    pthread_mutex_unlock(&lock);
    return ok;
}

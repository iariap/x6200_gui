/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Router RX procesado -> DAC (#364 / Epic #362). Ver rx_audio_live.h.
 */

#define _POSIX_C_SOURCE 200809L

#include "rx_audio_live.h"

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
} action_t;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;
static pthread_t       worker;
static bool            initialized;
static bool            running;
static bool            stop_worker;

static rx_audio_live_backend_t be;

static bool enabled;
static bool tx_active;
static bool generated_active;
static bool capture_fault;

static bool sink_open;
static bool sink_ready;

/* dac_selected: el backend confirmo que la ultima transicion DAC se aplico.
 * restore_unknown: el backend no pudo confirmar el estado fisico de una
 * transicion de entrada. En ambos casos existe la obligacion de volver a BASE. */
static bool dac_selected;
static bool restore_unknown;
static bool restore_failed;

static rx_audio_live_state_t state = RX_AUDIO_LIVE_BASE;

static int16_t ring[NBLOCKS][NSAMP];
static unsigned ring_head;
static unsigned ring_count;
static int16_t staging[NSAMP];
static size_t  staging_count;
static uint64_t last_submit_ms;
static uint64_t last_write_ms;
static bool     underrun_counted;
static uint64_t retry_after_ms;

static rx_audio_live_stats_t stats;

static uint64_t now_ms(void);

#ifdef X6200_RX_SPEAKER_QA
static rx_audio_live_qa_metrics_t qa_metrics;
static rx_audio_live_qa_event_t qa_events[RX_AUDIO_LIVE_QA_EVENT_RING];
static unsigned qa_event_head;
static unsigned qa_event_count;

static void qa_event_locked(uint32_t type, int32_t value) {
    if (qa_event_count == RX_AUDIO_LIVE_QA_EVENT_RING) {
        qa_metrics.events_lost++;
        return;
    }
    unsigned idx = (qa_event_head + qa_event_count) % RX_AUDIO_LIVE_QA_EVENT_RING;
    qa_events[idx].t_ms = now_ms();
    qa_events[idx].type = type;
    qa_events[idx].value = value;
    qa_event_count++;
}
#endif

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
#ifdef X6200_RX_SPEAKER_QA
    qa_metrics.ring_occupancy = 0;
#endif
}

static void push_block_locked(const int16_t *block) {
    unsigned dst;

    if (ring_count == NBLOCKS) {
        ring_head = (ring_head + 1u) % NBLOCKS;
        ring_count--;
        stats.dropped_blocks++;
    }

    dst = (ring_head + ring_count) % NBLOCKS;
    memcpy(ring[dst], block, sizeof(ring[dst]));
    ring_count++;
#ifdef X6200_RX_SPEAKER_QA
    qa_metrics.submitted_blocks++;
    qa_metrics.ring_occupancy = ring_count;
    if (ring_count > qa_metrics.ring_high_water) {
        qa_metrics.ring_high_water = ring_count;
    }
#endif
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

static void wait_locked(unsigned ms) {
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += (long) ms * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    (void) pthread_cond_timedwait(&cv, &lock, &ts);
}

/* Corre fuera del lock. Solo el worker (o shutdown despues del join) puede
 * entrar aca, por lo que nunca hay dos transiciones fisicas en curso. */
static void do_cleanup(rx_audio_live_suspend_t why) {
    bool need_restore;
    bool had_sink;

    pthread_mutex_lock(&lock);
    need_restore = dac_selected || restore_unknown;
    had_sink = sink_open;
#ifdef X6200_RX_SPEAKER_QA
    if (need_restore) {
        qa_event_locked(RX_AUDIO_LIVE_QA_EV_MUX_OFF_REQ, (int32_t) why);
    }
#endif
    pthread_mutex_unlock(&lock);

    if (need_restore) {
        int r = (be.mux_request != NULL) ? be.mux_request(be.ctx, false) : -1;

        pthread_mutex_lock(&lock);
        stats.base_requests++;
#ifdef X6200_RX_SPEAKER_QA
        qa_event_locked(RX_AUDIO_LIVE_QA_EV_MUX_OFF_RESULT, r);
#endif
        if (r == (int) RX_AUDIO_LIVE_MUX_OK) {
            dac_selected = false;
            restore_unknown = false;
            restore_failed = false;
#ifdef X6200_RX_SPEAKER_QA
            qa_metrics.mux_off_ok++;
#endif
        } else {
            /* No se sabe que el MUX haya vuelto: se conserva la obligacion y se
             * impide cualquier activacion nueva. */
            restore_failed = true;
            state = RX_AUDIO_LIVE_BASE_RESTORE_UNCONFIRMED;
            stats.restore_failures++;
        }
        pthread_mutex_unlock(&lock);
    }

    if (had_sink && be.close != NULL) {
        be.close(be.ctx);
    }

    pthread_mutex_lock(&lock);
#ifdef X6200_RX_SPEAKER_QA
    if (had_sink) {
        qa_event_locked(RX_AUDIO_LIVE_QA_EV_CLOSE, (int32_t) why);
    }
#endif
    sink_open = false;
    sink_ready = false;
    stats.closes += had_sink ? 1u : 0u;
    stats.last_suspend = why;
    clear_buffers_locked();
    if (!restore_failed) {
        state = RX_AUDIO_LIVE_BASE;
#ifdef X6200_RX_SPEAKER_QA
        qa_event_locked(RX_AUDIO_LIVE_QA_EV_BASE, (int32_t) why);
#endif
    }
    pthread_mutex_unlock(&lock);
}

static bool open_stream(void) {
    bool ok = (be.open != NULL) && be.open(be.ctx);

    pthread_mutex_lock(&lock);
    sink_open = ok;
    if (ok) {
        stats.opens++;
    }
#ifdef X6200_RX_SPEAKER_QA
    qa_event_locked(ok ? RX_AUDIO_LIVE_QA_EV_OPEN : RX_AUDIO_LIVE_QA_EV_OPEN_FAIL, 0);
#endif
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
    /* Buffer de un bloque que ya salio del ring pero cuyo write fue rechazado
     * por backpressure. Solo lo usa el worker. */
    int16_t pending[NSAMP];
    bool    pending_valid = false;
    bool    no_space = false;
    uint64_t pending_since_ms = 0;

    while (true) {
        action_t act = ACT_NONE;
        rx_audio_live_suspend_t why = RX_AUDIO_LIVE_SUSPEND_NONE;
        uint64_t now;

        pthread_mutex_lock(&lock);
        if (stop_worker) {
            pthread_mutex_unlock(&lock);
            break;
        }

        now = now_ms();

        /* Si una restauracion fallo, el router queda en error y no se reintenta
         * automaticamente: solo una nueva inicializacion explicita puede limpiar. */
        if (restore_failed) {
            wait_locked(RX_AUDIO_LIVE_WORKER_WAIT_MS);
            pthread_mutex_unlock(&lock);
            continue;
        }

        if (suspend_reason_locked(&why)) {
            bool cleanup = (state != RX_AUDIO_LIVE_BASE) || sink_open ||
                           dac_selected || restore_unknown || pending_valid;
            if (!cleanup) {
                wait_locked(RX_AUDIO_LIVE_WORKER_WAIT_MS);
                pthread_mutex_unlock(&lock);
                continue;
            }
            pending_valid = false;
            pthread_mutex_unlock(&lock);
            do_cleanup(why);
            continue;
        }

        if (retry_after_ms != 0 && now < retry_after_ms) {
            wait_locked(RX_AUDIO_LIVE_WORKER_WAIT_MS);
            pthread_mutex_unlock(&lock);
            continue;
        }
        retry_after_ms = 0;

        if (!sink_open) {
            act = ACT_OPEN;
        } else if (!sink_ready) {
            act = ACT_READY;
        } else if (state == RX_AUDIO_LIVE_BASE) {
            bool ready_gate = (be.mux_ready != NULL) && be.mux_ready(be.ctx);
            if (ready_gate && ring_count >= RX_AUDIO_LIVE_START_BLOCKS &&
                !stale_locked(now)) {
                state = RX_AUDIO_LIVE_PREPARE_DAC;
                act = ACT_MUX_ON;
#ifdef X6200_RX_SPEAKER_QA
                qa_event_locked(RX_AUDIO_LIVE_QA_EV_MUX_ON_REQ, 0);
#endif
            }
        } else if (state == RX_AUDIO_LIVE_PREPARE_DAC) {
            /* La transicion se resuelve en la misma vuelta de ACT_MUX_ON. */
        } else if (state == RX_AUDIO_LIVE_RX_DAC) {
            if (pending_valid) {
                act = ACT_WRITE;
            } else if (pop_block_locked(pending)) {
                pending_valid = true;
                pending_since_ms = 0;
                act = ACT_WRITE;
            } else if (sink_ready && !stale_locked(now)) {
                if (!underrun_counted && last_write_ms != 0 &&
                    now > last_write_ms + RX_AUDIO_LIVE_BLOCK_SAMPLES * 1000u / RX_AUDIO_LIVE_RATE + 100u) {
                    stats.underruns++;
                    underrun_counted = true;
                }
            }
        } else if (state == RX_AUDIO_LIVE_DEGRADED_BASE) {
            pending_valid = false;
            pthread_mutex_unlock(&lock);
            do_cleanup(RX_AUDIO_LIVE_SUSPEND_SINK);
            continue;
        } else if (state == RX_AUDIO_LIVE_BASE_RESTORE_UNCONFIRMED) {
            wait_locked(RX_AUDIO_LIVE_WORKER_WAIT_MS);
            pthread_mutex_unlock(&lock);
            continue;
        }

        if (act == ACT_NONE) {
            wait_locked(RX_AUDIO_LIVE_WORKER_WAIT_MS);
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
                int wr = (be.write != NULL) ? be.write(be.ctx, pending, NSAMP)
                                            : (int) RX_AUDIO_LIVE_WRITE_ERROR;

                pthread_mutex_lock(&lock);
                if (wr == (int) RX_AUDIO_LIVE_WRITE_OK) {
                    stats.writes++;
                    last_write_ms = now_ms();
                    underrun_counted = false;
                    pending_valid = false;
                    pending_since_ms = 0;
                } else if (wr == (int) RX_AUDIO_LIVE_WRITE_NO_SPACE) {
                    no_space = true;
                    if (pending_since_ms == 0) {
                        pending_since_ms = now_ms();
                    }
#ifdef X6200_RX_SPEAKER_QA
                    {
                        uint64_t age = now_ms() - pending_since_ms;
                        if (age > qa_metrics.pending_max_ms) {
                            qa_metrics.pending_max_ms = (uint32_t) age;
                        }
                    }
#endif
                    if (pending_since_ms != 0 &&
                        now_ms() - pending_since_ms > RX_AUDIO_LIVE_BACKPRESSURE_MS) {
                        /* El backend no sostiene el objetivo de latencia: no se
                         * acumula audio viejo; se vuelve a BASE. */
                        stats.backpressure++;
                        stats.last_suspend = RX_AUDIO_LIVE_SUSPEND_BACKPRESSURE;
                        state = RX_AUDIO_LIVE_DEGRADED_BASE;
#ifdef X6200_RX_SPEAKER_QA
                        qa_metrics.degraded_transitions++;
                        qa_event_locked(RX_AUDIO_LIVE_QA_EV_DEGRADED,
                                        (int32_t) RX_AUDIO_LIVE_SUSPEND_BACKPRESSURE);
#endif
                        pending_valid = false;
                    }
                } else {
                    stats.last_suspend = RX_AUDIO_LIVE_SUSPEND_SINK;
                    state = RX_AUDIO_LIVE_DEGRADED_BASE;
                    pending_valid = false;
                }
                pthread_mutex_unlock(&lock);
                break;
            }

            case ACT_MUX_ON: {
                int r = (be.mux_request != NULL) ? be.mux_request(be.ctx, true)
                                                 : (int) RX_AUDIO_LIVE_MUX_UNKNOWN;

                pthread_mutex_lock(&lock);
                stats.dac_requests++;
#ifdef X6200_RX_SPEAKER_QA
                qa_event_locked(RX_AUDIO_LIVE_QA_EV_MUX_ON_RESULT, r);
#endif
                if (r == (int) RX_AUDIO_LIVE_MUX_OK) {
                    /* La obligacion de restaurar nace apenas el backend confirma
                     * la seleccion, aunque un disable/TX haya llegado mientras
                     * la llamada estaba en curso. */
                    dac_selected = true;
                    restore_unknown = false;
                    if (enabled && !tx_active && !generated_active && !capture_fault) {
                        state = RX_AUDIO_LIVE_RX_DAC;
#ifdef X6200_RX_SPEAKER_QA
                        qa_metrics.mux_on_ok++;
                        qa_event_locked(RX_AUDIO_LIVE_QA_EV_RX_DAC, 0);
#endif
                    } else {
                        stats.last_suspend = RX_AUDIO_LIVE_SUSPEND_MUX;
                        state = RX_AUDIO_LIVE_DEGRADED_BASE;
                    }
                } else if (r == (int) RX_AUDIO_LIVE_MUX_NOT_ENTERED) {
                    /* No hubo transicion fisica: se puede declarar BASE. */
                    state = RX_AUDIO_LIVE_BASE;
                    stats.last_suspend = RX_AUDIO_LIVE_SUSPEND_MUX;
                    retry_after_ms = now_ms() + 50u;
                } else {
                    /* Resultado incierto: no se declara BASE. */
                    restore_unknown = true;
                    stats.last_suspend = RX_AUDIO_LIVE_SUSPEND_RESTORE;
                    state = RX_AUDIO_LIVE_DEGRADED_BASE;
                }
                pthread_mutex_unlock(&lock);
                break;
            }

            case ACT_NONE:
                break;
        }

        if (no_space) {
            /* No martillar el mainloop de PulseAudio: el sink necesita correr
             * para liberar espacio. 5 ms de respiro y se reintenta. */
            struct timespec nap = { .tv_sec = 0,
                                    .tv_nsec = RX_AUDIO_LIVE_NO_SPACE_WAIT_MS * 1000000L };
            nanosleep(&nap, NULL);
            no_space = false;
        }
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
        restore_unknown = false;
        restore_failed = false;
        state = RX_AUDIO_LIVE_BASE;
        last_submit_ms = 0;
        last_write_ms = 0;
        underrun_counted = false;
        retry_after_ms = 0;
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

    /* El worker ya no corre. Si quedo una seleccion/restauracion pendiente, se
     * intenta cerrar la obligacion; si falla, el estado de error se conserva. */
    pthread_mutex_lock(&lock);
    bool need_cleanup = dac_selected || restore_unknown || sink_open;
    pthread_mutex_unlock(&lock);
    if (need_cleanup) {
        do_cleanup(RX_AUDIO_LIVE_SUSPEND_SHUTDOWN);
    }

    pthread_mutex_lock(&lock);
    initialized = false;
    stop_worker = false;
    enabled = false;
    tx_active = false;
    generated_active = false;
    capture_fault = false;
    sink_open = false;
    sink_ready = false;
    clear_buffers_locked();
    if (!restore_failed) {
        dac_selected = false;
        restore_unknown = false;
        state = RX_AUDIO_LIVE_BASE;
    }
    pthread_mutex_unlock(&lock);
}

bool rx_audio_live_set_enabled(bool on) {
    bool ok = false;

    pthread_mutex_lock(&lock);
    if (!initialized) {
        ok = false;
    } else if (on && (restore_failed || state == RX_AUDIO_LIVE_BASE_RESTORE_UNCONFIRMED)) {
        ok = false;
    } else {
        enabled = on;
        if (!on) {
            clear_buffers_locked();
        }
        pthread_cond_broadcast(&cv);
        ok = true;
    }
    pthread_mutex_unlock(&lock);
    return ok;
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
    if (!enabled || tx_active || generated_active || restore_failed ||
        state == RX_AUDIO_LIVE_BASE_RESTORE_UNCONFIRMED) {
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
        case RX_AUDIO_LIVE_BASE_RESTORE_UNCONFIRMED: return "BASE_RESTORE_UNCONFIRMED";
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
        case RX_AUDIO_LIVE_SUSPEND_BACKPRESSURE: return "backpressure";
        case RX_AUDIO_LIVE_SUSPEND_RESTORE: return "restore";
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
    ok = initialized && !restore_failed && !restore_unknown &&
         be.open != NULL && be.write != NULL && be.mux_request != NULL &&
         be.mux_ready != NULL && be.mux_ready(be.ctx);
    pthread_mutex_unlock(&lock);
    return ok;
}

bool rx_audio_live_restore_unconfirmed(void) {
    bool v;

    pthread_mutex_lock(&lock);
    v = restore_failed || state == RX_AUDIO_LIVE_BASE_RESTORE_UNCONFIRMED;
    pthread_mutex_unlock(&lock);
    return v;
}

bool rx_audio_live_tx_active(void) {
    bool v;

    pthread_mutex_lock(&lock);
    v = tx_active;
    pthread_mutex_unlock(&lock);
    return v;
}

bool rx_audio_live_generated_active(void) {
    bool v;

    pthread_mutex_lock(&lock);
    v = generated_active;
    pthread_mutex_unlock(&lock);
    return v;
}

#ifdef X6200_RX_SPEAKER_QA
void rx_audio_live_qa_reset_metrics(void) {
    pthread_mutex_lock(&lock);
    memset(&qa_metrics, 0, sizeof(qa_metrics));
    qa_event_head = 0;
    qa_event_count = 0;
    pthread_mutex_unlock(&lock);
}

void rx_audio_live_qa_get_metrics(rx_audio_live_qa_metrics_t *out) {
    if (out == NULL) {
        return;
    }
    pthread_mutex_lock(&lock);
    *out = qa_metrics;
    out->ring_occupancy = ring_count;
    out->pending_samples = (uint32_t) (ring_count * NSAMP + staging_count);
    out->state = (uint32_t) state;
    pthread_mutex_unlock(&lock);
}

size_t rx_audio_live_qa_drain_events(rx_audio_live_qa_event_t *out, size_t max) {
    size_t n = 0;

    if (out == NULL || max == 0) {
        return 0;
    }
    pthread_mutex_lock(&lock);
    while (n < max && qa_event_count > 0) {
        out[n++] = qa_events[qa_event_head];
        qa_event_head = (qa_event_head + 1u) % RX_AUDIO_LIVE_QA_EVENT_RING;
        qa_event_count--;
    }
    pthread_mutex_unlock(&lock);
    return n;
}

const char *rx_audio_live_qa_event_name(uint32_t type) {
    switch (type) {
        case RX_AUDIO_LIVE_QA_EV_OPEN: return "open";
        case RX_AUDIO_LIVE_QA_EV_OPEN_FAIL: return "open_fail";
        case RX_AUDIO_LIVE_QA_EV_CLOSE: return "close";
        case RX_AUDIO_LIVE_QA_EV_MUX_ON_REQ: return "mux_on_req";
        case RX_AUDIO_LIVE_QA_EV_MUX_ON_RESULT: return "mux_on_result";
        case RX_AUDIO_LIVE_QA_EV_MUX_OFF_REQ: return "mux_off_req";
        case RX_AUDIO_LIVE_QA_EV_MUX_OFF_RESULT: return "mux_off_result";
        case RX_AUDIO_LIVE_QA_EV_RX_DAC: return "rx_dac";
        case RX_AUDIO_LIVE_QA_EV_DEGRADED: return "degraded";
        case RX_AUDIO_LIVE_QA_EV_BASE: return "base";
        default: return "?";
    }
}
#endif

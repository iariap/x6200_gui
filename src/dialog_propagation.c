/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - App "Propagation" (issue #145)
 *
 *  App a demanda para la banda actual: mide los beacons del IBP (NCDXF/IARU) que
 *  transmiten en los proximos slots y dice si la banda esta abierta, con la
 *  evidencia a la vista. Es SOLO RX: nunca transmite, y al terminar/cancelar/cerrar
 *  devuelve el VFO exactamente como estaba.
 *
 *  El barrido corre en un hilo aparte (la GUI sigue atendiendo encoders y menu).
 *  El hilo NO toca LVGL: publica el progreso en `status` bajo `lock` y despierta a
 *  la GUI con scheduler_put_noargs(), el equivalente seguro a lv_async_call de esta
 *  ROM (LVGL 8 crea un timer desde el hilo que llama, y eso no es thread-safe; ver
 *  docs/HALLAZGOS.md 58). El unico timer de la app (1 s) existe solo mientras el
 *  barrido corre con la ventana abierta: cerrada la app no queda ningun timer ni
 *  hilo propio (cero consumo en reposo).
 *
 *  Medicion (ver src/propagation.c): por cada beacon del plan, se sintoniza (solo
 *  RX), se mide el piso de ruido a +5 kHz (la banda del IBP tiene un beacon en
 *  cada slot: no hay hueco libre en la frecuencia del beacon), y durante el slot
 *  se toman los maximos de las ventanas de la escalera 100/10/1/0.1 W.
 */

#include "dialog_propagation.h"

#include "buttons.h"
#include "cfg/cfg.h"
#include "clock.h"
#include "cluster/client.h"
#include "dialog.h"
#include "dxcc.h"
#include "events.h"
#include "fonts/font_select.h"
#include "keyboard.h"
#include "main_screen.h"
#include "meter.h"
#include "msg.h"
#include "params/params.h"
#include "propagation.h"
#include "qth/qth.h"
#include "radio.h"
#include "scheduler.h"
#include "styles.h"
#include "theme.h"
#include "util.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PROP_UI_TICK_MS         1000
#define PROP_NOISE_OFFSET_HZ    5000        /* piso de ruido fuera de la frecuencia del beacon */
#define PROP_NOISE_MS           1000
#define PROP_SAMPLE_MS          50
#define PROP_PREPARE_MS         400         /* el paso 1 se ve aunque la banda no tenga beacons */
#define PROP_SYNC_MS            600
#define PROP_EVIDENCE_ROWS      PROPAGATION_PLAN_MAX

#define VF(n) (theme_shape.vision ? font_down(&sony_##n, VISION_FONT_STEPS - 1) : &sony_##n)

typedef enum {
    PROP_PHASE_IDLE = 0,    /* paso 1: preparado, esperando Run */
    PROP_PHASE_PREPARE,     /* paso 1: preparando */
    PROP_PHASE_SYNC,        /* paso 2: reloj y transmitiendo ahora */
    PROP_PHASE_MEASURE,     /* paso 3: barrido */
    PROP_PHASE_RESULT,      /* paso 4: veredicto */
    PROP_PHASE_CANCELLED,   /* cancelado por el operador */
} prop_phase_t;

typedef struct {
    int     beacon;
    int64_t slot_start;
    int     noise_db;
    int     step_max_db[PROPAGATION_LADDER_STEPS];
    int     snr[PROPAGATION_LADDER_STEPS];
    bool    step_done[PROPAGATION_LADDER_STEPS];
    int     ladder;
    bool    done;
} prop_meas_t;

typedef struct {
    prop_phase_t phase;
    int          band;              /* indice IBP, -1 si la banda no tiene */
    char         band_name[32];
    bool         clock_synced;
    int64_t      start_epoch;
    int          plan[PROPAGATION_PLAN_MAX];
    int64_t      plan_slot[PROPAGATION_PLAN_MAX];
    int          plan_n;
    int          plan_i;
    int          windows;
    int          ladder;
    int          last_noise;
    prop_meas_t  meas[PROPAGATION_PLAN_MAX];
    int          measured;
    int          heard;
    int          best;              /* indice en meas, -1 si ninguno */
    int          best_snr;
    propagation_verdict_t verdict;
    bool         verdict_ready;
    char         cluster[3][96];
    int          cluster_n;
    char         direction[160];
    char         note[192];
    bool         listening;
    int          listen_idx;
} prop_status_t;

static pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t        worker;
static bool             worker_started = false;
static atomic_bool      stop_req;
static prop_status_t    status;
static bool             ui_alive = false;
static lv_timer_t      *tick_timer = NULL;
static bool             sweep_ended = false;

static int32_t          saved_freq = 0;

static lv_obj_t        *lbl_title;
static lv_obj_t        *lbl_band;
static lv_obj_t        *lbl_step;
static lv_obj_t        *lbl_status;
static lv_obj_t        *lbl_evidence[PROP_EVIDENCE_ROWS];
static lv_obj_t        *lbl_verdict;
static lv_obj_t        *lbl_detail;
static lv_obj_t        *bar;

static void construct_cb(lv_obj_t *parent);
static void draw_bg_cb(lv_event_t *e);
static void destruct_cb(void);
static void ui_deleted_cb(lv_event_t *e);
static void key_cb(lv_event_t *e);
static void render(void);
static void ui_wake(void *arg);
static void tick_cb(lv_timer_t *t);
static void start_sweep(void);
static void cancel_sweep(void);
static void sweep_ui_end(void);
static void listen_stop(void);

static char run_label[8] = "Run";

static void dialog_propagation_run_cb(button_item_t *item);
static void dialog_propagation_repeat_cb(button_item_t *item);
static void dialog_propagation_listen_cb(button_item_t *item);
static void dialog_propagation_thresholds_cb(button_item_t *item);
static void dialog_propagation_close_cb(button_item_t *item);

static const char *thresholds_label_fn(void);

static button_item_t btn_run = {
    .type  = BTN_TEXT,
    .label = "Run",
    .press = dialog_propagation_run_cb,
};
static button_item_t btn_repeat = {
    .type  = BTN_TEXT,
    .label = "Repeat",
    .press = dialog_propagation_repeat_cb,
};
static button_item_t btn_listen = {
    .type  = BTN_TEXT,
    .label = "Listen",
    .press = dialog_propagation_listen_cb,
};
static button_item_t btn_thresholds = {
    .type     = BTN_TEXT_FN,
    .label_fn = thresholds_label_fn,
    .press    = dialog_propagation_thresholds_cb,
};
static button_item_t btn_close = {
    .type  = BTN_TEXT,
    .label = "Close",
    .press = dialog_propagation_close_cb,
};

static buttons_page_t btn_page = {
    {
     &btn_run,
     &btn_repeat,
     &btn_listen,
     &btn_thresholds,
     &btn_close,
     }
};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .btn_page     = &btn_page,
    .key_cb       = key_cb,
    .restore_radio = true,
};

dialog_t *dialog_propagation = &dialog;

/* --- Helpers de tiempo ---------------------------------------------------- */

static int64_t mono_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Duerme en tramos cortos para atender el Cancelar. False si lo cancelaron. */
static bool sleep_ms(int ms) {
    int64_t end = mono_ms() + ms;

    while (mono_ms() < end) {
        if (atomic_load_explicit(&stop_req, memory_order_relaxed)) {
            return false;
        }
        usleep(20000);
    }

    return !atomic_load_explicit(&stop_req, memory_order_relaxed);
}

/* Espera a que el reloj UTC llegue a `target` (epoch, segundos). La resolucion de
 * time() es de 1 s: se duerme en tramos de 20 ms solo en el ultimo segundo. */
static bool wait_epoch(int64_t target) {
    for (;;) {
        if (atomic_load_explicit(&stop_req, memory_order_relaxed)) {
            return false;
        }

        int64_t now = (int64_t) time(NULL);

        if (now >= target) {
            return true;
        }

        usleep((target - now) > 2 ? 200000 : 20000);
    }
}

/* --- Estado compartido ---------------------------------------------------- */

/* Copia el estado para la GUI; el llamador ya tiene el lock. */
static void status_copy(prop_status_t *out) {
    pthread_mutex_lock(&lock);
    *out = status;
    pthread_mutex_unlock(&lock);
}

/* El hilo del barrido avisa a la GUI. scheduler_put_noargs() es thread-safe (LVGL
 * 8 no lo es: ver el comentario del principio). */
static void status_changed(void) {
    scheduler_put_noargs(ui_wake);
}

static void measure_reset(prop_meas_t *m) {
    memset(m, 0, sizeof(*m));
    m->noise_db = 0;

    for (int k = 0; k < PROPAGATION_LADDER_STEPS; k++) {
        m->step_max_db[k] = INT16_MIN;
        m->snr[k]         = INT16_MIN;
    }
}

/* --- Medicion (hilo aparte) ----------------------------------------------- */

/* Maximo del S-meter en una ventana de `len_ms` que arranca en `start_ms` (mono). */
static int sample_window(int64_t start_ms, int len_ms, bool *sampled) {
    int     max = INT16_MIN;
    int64_t end = start_ms + len_ms;

    while (mono_ms() < end) {
        if (atomic_load_explicit(&stop_req, memory_order_relaxed)) {
            return max;
        }

        int16_t db = meter_get_raw_db();

        if (db > max) {
            max = db;
        }
        if (sampled) {
            *sampled = true;
        }

        usleep(PROP_SAMPLE_MS * 1000);
    }

    return max;
}

/* Piso de ruido de la banda: promedio del S-meter a +5 kHz, lejos del beacon. */
static int measure_noise(int freq_hz, int64_t slot_start, bool *ok) {
    int64_t now = (int64_t) time(NULL);

    *ok = false;

    if (now >= slot_start - 1) {
        return status.last_noise;
    }

    if (now < slot_start - 4) {
        if (!wait_epoch(slot_start - 4)) {
            return status.last_noise;
        }
    }

    radio_set_freq(freq_hz + PROP_NOISE_OFFSET_HZ);

    int64_t available = (slot_start - (int64_t) time(NULL) - 1) * 1000;
    int     len       = available < PROP_NOISE_MS ? (int) available : PROP_NOISE_MS;

    if (len < 200) {
        len = 200;
    }

    int64_t end = mono_ms() + len;
    int     n   = 0;
    int64_t sum = 0;

    while (mono_ms() < end) {
        if (atomic_load_explicit(&stop_req, memory_order_relaxed)) {
            return status.last_noise;
        }

        sum += meter_get_raw_db();
        n++;
        usleep(PROP_SAMPLE_MS * 1000);
    }

    /* De vuelta a la frecuencia del beacon: el worker no puede tocar el subject
     * (lo hace radio_set_freq, que solo habla con el hardware) y el lock del
     * dial impide que el operador la mueva mientras dura el barrido. */
    radio_set_freq(freq_hz);

    if (n == 0) {
        return status.last_noise;
    }

    *ok = true;

    return (int) ((sum + n / 2) / n);
}

/* Un beacon: espera su slot, mide la escalera y publica el progreso. */
static void measure_beacon(int index, int freq_hz) {
    prop_meas_t m;

    measure_reset(&m);
    m.beacon     = status.plan[index];
    m.slot_start = status.plan_slot[index];

    int64_t station_now = (int64_t) time(NULL);

    if (station_now >= m.slot_start - 1) {
        /* Se llego tarde (o el slot ya empezo): no se puede medir desde el
         * principio, asi que este beacon no cuenta. */
        pthread_mutex_lock(&lock);
        status.meas[index] = m;
        status.meas[index].done = true;
        status.plan_i = index + 1;
        pthread_mutex_unlock(&lock);
        status_changed();
        return;
    }

    bool noise_ok = false;
    int  noise    = measure_noise(freq_hz, m.slot_start, &noise_ok);

    m.noise_db = noise;

    if (!noise_ok) {
        /* Sin piso medido no se inventa: se usa el ultimo y la pantalla lo dice. */
        m.noise_db = status.last_noise;
    }

    pthread_mutex_lock(&lock);
    status.last_noise = m.noise_db;
    status.plan_i     = index;
    status.windows    = 0;
    status.ladder     = 0;
    status.meas[index] = m;
    pthread_mutex_unlock(&lock);
    status_changed();

    if (!wait_epoch(m.slot_start)) {
        return;
    }

    if (radio_get_state() != RADIO_RX) {
        return;
    }

    int64_t slot_mono = mono_ms();

    for (int k = 0; k < PROPAGATION_LADDER_STEPS; k++) {
        int start_ms, len_ms;

        if (!propagation_ladder_window(k, &start_ms, &len_ms)) {
            break;
        }

        int64_t target = slot_mono + start_ms;

        while (mono_ms() < target) {
            if (atomic_load_explicit(&stop_req, memory_order_relaxed)) {
                return;
            }
            usleep(20000);
        }

        bool sampled = false;
        int  max_db  = sample_window(mono_ms(), len_ms, &sampled);

        if (!sampled) {
            break;
        }

        int snr = propagation_snr(max_db, m.noise_db);

        m.step_max_db[k] = max_db;
        m.snr[k]         = snr;
        m.step_done[k]   = true;

        if (snr >= PROPAGATION_DETECT_DB) {
            /* Solo se cuentan los escalones SEGUIDOS desde el primero. */
            if (k == m.ladder) {
                m.ladder++;
            }
        } else {
            /* Un hueco corta la escalera: lo que sigue es mas debil y no se
             * puede atribuir a un escalon que no se escucho. */
            for (int j = k + 1; j < PROPAGATION_LADDER_STEPS; j++) {
                m.step_done[j] = false;
            }
            break;
        }

        pthread_mutex_lock(&lock);
        status.meas[index] = m;
        status.windows     = k + 1;
        status.ladder      = m.ladder;
        pthread_mutex_unlock(&lock);
        status_changed();
    }

    m.done = true;

    pthread_mutex_lock(&lock);
    status.meas[index] = m;
    status.windows     = PROPAGATION_LADDER_STEPS;
    status.ladder      = m.ladder;
    status.plan_i      = index + 1;
    pthread_mutex_unlock(&lock);
    status_changed();
}

/* Arma la evidencia complementaria del cluster (dato local, sin red). */
static void fill_cluster(const prop_status_t *st) {
    cluster_spot_t spots[24];
    uint32_t       gen = 0;
    int            band_idx = cluster_band_of((int32_t) saved_freq);

    if (!params.propagation_cluster.x || band_idx < 0) {
        return;
    }

    int n = cluster_client_snapshot(spots, 24, &gen);

    if (n <= 0) {
        return;
    }

    int now_utc_min = (int) (((int64_t) time(NULL) / 60) % 1440);
    int out         = 0;

    for (int i = n - 1; i >= 0 && out < 3; i--) {
        if (cluster_band_of(spots[i].freq_hz) != band_idx) {
            continue;
        }

        int age = spots[i].when > 0
                  ? (int) (((int64_t) time(NULL) - spots[i].when) / 60)
                  : (int) cluster_age_min(spots[i].utc_min, (uint16_t) now_utc_min);

        snprintf(status.cluster[out], sizeof(status.cluster[out]),
                 "%s %u.%03u MHz  |  hace %d min", spots[i].dx,
                 (unsigned) (spots[i].freq_hz / 1000000),
                 (unsigned) (spots[i].freq_hz / 1000 % 1000), age);
        out++;
    }

    status.cluster_n = out;
}

/* Direccion del mejor beacon escuchado: DXCC/grilla -> rumbo y distancia. */
static void fill_direction(const prop_status_t *st) {
    if (st->band < 0 || st->best < 0 || st->meas[st->best].ladder < 1) {
        return;
    }

    const char *call = propagation_ibp_call(st->meas[st->best].beacon);
    const char *grid = propagation_ibp_grid(st->meas[st->best].beacon);
    const char *site = propagation_ibp_site(st->meas[st->best].beacon);

    if (!call || !grid) {
        return;
    }

    double my_lat, my_lon, dx_lat, dx_lon;

    if (!qth_grid_check(params.qth.x)) {
        snprintf(status.direction, sizeof(status.direction),
                 "Set QTH in Settings to calculate direction.");
        return;
    }

    qth_str_to_pos(params.qth.x, &my_lat, &my_lon);
    qth_str_to_pos(grid, &dx_lat, &dx_lon);

    int    bearing = propagation_bearing(my_lat, my_lon, dx_lat, dx_lon);
    double km      = qth_pos_dist(my_lat, my_lon, dx_lat, dx_lon);
    const char *entity = NULL;
    const char *prefix = NULL;

    if (!dxcc_lookup(call, &entity, &prefix) || !entity) {
        entity = site;
    }

    snprintf(status.direction, sizeof(status.direction),
             "Hacia %s (%s): %s %d deg, %.0f km.",
             entity ? entity : "el beacon", grid,
             propagation_compass(bearing), bearing, km);
}

/* Cierre del barrido: veredicto, cobertura, direccion y cluster. */
static void build_result(bool cancelled) {
    int measured = 0;
    int heard    = 0;
    int best     = -1;
    int best_snr = INT16_MIN;

    for (int i = 0; i < status.plan_n; i++) {
        const prop_meas_t *m = &status.meas[i];

        if (!m->done) {
            continue;
        }

        measured++;

        if (m->ladder > 0) {
            heard++;

            if (m->ladder > (best >= 0 ? status.meas[best].ladder : 0) ||
                (best < 0) ||
                (m->ladder == status.meas[best].ladder && m->snr[0] > best_snr)) {
                best     = i;
                best_snr = m->snr[0];
            }
        }

        if (m->snr[0] > best_snr) {
            best_snr = m->snr[0];
            if (best < 0) {
                best = i;
            }
        }
    }

    status.measured = measured;
    status.heard    = heard;
    status.best     = (heard > 0) ? best : -1;
    status.best_snr = (best_snr == INT16_MIN) ? 0 : best_snr;

    int open = params.propagation_open_db.x;
    int marg = params.propagation_marginal_db.x;

    if (status.band < 0) {
        status.verdict = PROPAGATION_VERDICT_NO_DATA;
        snprintf(status.note, sizeof(status.note),
                 "No IBP beacons on %s. IARU IBP beacons start at 14 MHz; "
                 "cluster spots follow if available.", status.band_name);
    } else if (measured == 0) {
        status.verdict = cancelled ? PROPAGATION_VERDICT_NO_DATA : PROPAGATION_VERDICT_NO_DATA;
        snprintf(status.note, sizeof(status.note),
                 cancelled ? "Cancelled by operator. VFO restored."
                           : "No measurement slot available. VFO restored.");
    } else {
        status.verdict = propagation_verdict(status.best_snr, heard > 0, open, marg);

        if (heard == 0) {
            snprintf(status.note, sizeof(status.note),
                     "No beacon heard in its slot. Silence does not prove the band is "
                     "closed (QRM/QRN possible).");
        } else if (!status.clock_synced) {
            snprintf(status.note, sizeof(status.note),
                     "Clock not synchronized: beacon timing may be inaccurate.");
        } else {
            snprintf(status.note, sizeof(status.note),
                     "Thresholds: open >= %d dB, marginal >= %d dB.", open, marg);
        }
    }

    fill_cluster(&status);

    if (status.band < 0 && status.cluster_n == 0) {
        snprintf(status.note + strlen(status.note),
                 sizeof(status.note) - strlen(status.note),
                 " No recent cluster spots for this band.");
    }

    fill_direction(&status);

    status.verdict_ready = true;
    status.phase         = cancelled ? PROP_PHASE_CANCELLED : PROP_PHASE_RESULT;
}

/* El equipo dejo de estar en RX (PTT del operador, un barrido de ROE...): el
 * S-meter ya no mide lo que la app cree, asi que se corta y se vuelve el VFO. */
static void abort_on_tx(void) {
    radio_set_freq(saved_freq);

    pthread_mutex_lock(&lock);
    build_result(true);
    snprintf(status.note, sizeof(status.note),
             "Radio left RX: scan cancelled. VFO restored.");
    status.phase = PROP_PHASE_CANCELLED;
    pthread_mutex_unlock(&lock);

    status_changed();
}

static void *sweep_thread(void *arg) {
    (void) arg;

    if (!sleep_ms(PROP_PREPARE_MS)) {
        return NULL;
    }

    if (status.band >= 0) {
        pthread_mutex_lock(&lock);
        status.phase = PROP_PHASE_SYNC;
        pthread_mutex_unlock(&lock);
        status_changed();

        /* El paso 2 se ve, pero sin comerse el primer slot del plan. */
        int wait_first = status.plan_n > 0
                         ? (int) (status.plan_slot[0] - (int64_t) time(NULL)) : 99;
        int sync_ms    = wait_first < 3 ? 200 : PROP_SYNC_MS;

        if (!sleep_ms(sync_ms)) {
            return NULL;
        }

        pthread_mutex_lock(&lock);
        status.phase = PROP_PHASE_MEASURE;
        pthread_mutex_unlock(&lock);
        status_changed();

        for (int i = 0; i < status.plan_n; i++) {
            if (atomic_load_explicit(&stop_req, memory_order_relaxed)) {
                return NULL;
            }

            if (radio_get_state() != RADIO_RX) {
                abort_on_tx();
                return NULL;
            }

            measure_beacon(i, (int) propagation_ibp_freq(status.band));

            if (atomic_load_explicit(&stop_req, memory_order_relaxed)) {
                return NULL;
            }
        }
    }

    /* El VFO vuelve SIEMPRE a la frecuencia con la que se abrio la app. */
    radio_set_freq(saved_freq);

    pthread_mutex_lock(&lock);
    build_result(false);
    pthread_mutex_unlock(&lock);

    status_changed();

    return NULL;
}

/* --- Ciclo de vida de la GUI ---------------------------------------------- */

static void join_worker(void) {
    if (worker_started) {
        pthread_join(worker, NULL);
        worker_started = false;
    }
}

static void sweep_ui_end(void) {
    if (sweep_ended) {
        return;
    }

    sweep_ended = true;
    join_worker();

    if (tick_timer) {
        lv_timer_del(tick_timer);
        tick_timer = NULL;
    }

    main_screen_lock_band(false);
    main_screen_lock_mode(false);
    main_screen_lock_freq(false);
}

/* Plan del barrido para la banda actual: los PROPAGATION_PLAN_MAX beacons que
 * transmiten despues del slot que corre en `epoch`, con el epoch de su slot. */
static void prepare_plan(int64_t epoch) {
    status.plan_n = 0;

    if (status.band < 0) {
        return;
    }

    status.plan_n = propagation_plan(epoch, status.band, status.plan);

    for (int i = 0; i < status.plan_n; i++) {
        int wait = 0;

        if (!propagation_beacon_slot(epoch, status.band, status.plan[i],
                                     &status.plan_slot[i], &wait)) {
            status.plan_slot[i] = epoch;
        }
    }
}

static void start_sweep(void) {
    if (worker_started) {
        return;
    }

    listen_stop();

    /* La frecuencia y el modo quedan bloqueados mientras la app esta abierta: el
     * barrido mueve el VFO por hardware (radio_set_freq) y el unico camino de
     * vuelta es el que guardamos aca. Asi el restore es exacto aunque el operador
     * toque los encoders (el volumen y el resto siguen andando). */
    saved_freq = subject_get_int(cfg_cur.fg_freq);

    if (!radio_check_freq(saved_freq)) {
        msg_update_text_fmt("Propagation: invalid current frequency");
        return;
    }

    memset(&status, 0, sizeof(status));
    status.phase        = PROP_PHASE_PREPARE;
    status.band         = propagation_ibp_band_of_freq((uint32_t) saved_freq);
    status.clock_synced = clock_time_synced();
    status.start_epoch  = (int64_t) time(NULL);
    status.best         = -1;
    status.last_noise   = meter_get_raw_db();
    status.plan_n       = 0;

    snprintf(status.band_name, sizeof(status.band_name), "%s", cfg_band_label_get());
    if (status.band_name[0] == '\0') {
        snprintf(status.band_name, sizeof(status.band_name), "current band");
    }

    prepare_plan(status.start_epoch);

    for (int i = 0; i < PROPAGATION_PLAN_MAX; i++) {
        measure_reset(&status.meas[i]);
    }

    sweep_ended = false;
    atomic_store(&stop_req, false);

    if (tick_timer) {
        lv_timer_del(tick_timer);
    }
    tick_timer = lv_timer_create(tick_cb, PROP_UI_TICK_MS, NULL);

    main_screen_lock_freq(true);
    main_screen_lock_band(true);
    main_screen_lock_mode(true);

    render();

    if (pthread_create(&worker, NULL, sweep_thread, NULL) != 0) {
        status.phase = PROP_PHASE_IDLE;
        snprintf(status.note, sizeof(status.note), "Could not start scan worker.");
        join_worker();
        sweep_ui_end();
        render();
        return;
    }

    worker_started = true;
}

static void cancel_sweep(void) {
    if (!worker_started && status.phase != PROP_PHASE_MEASURE && status.phase != PROP_PHASE_SYNC) {
        return;
    }

    atomic_store(&stop_req, true);
    join_worker();

    radio_set_freq(saved_freq);

    pthread_mutex_lock(&lock);
    build_result(true);
    pthread_mutex_unlock(&lock);

    sweep_ui_end();
    render();
}

static void listen_stop(void) {
    if (!status.listening) {
        return;
    }

    radio_set_freq(saved_freq);

    pthread_mutex_lock(&lock);
    status.listening = false;
    status.listen_idx = -1;
    pthread_mutex_unlock(&lock);

    button_update_label(&btn_listen, "Listen");
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    ui_alive   = true;
    sweep_ended = true;

    lv_obj_add_event_cb(dialog.obj, ui_deleted_cb, LV_EVENT_DELETE, NULL);

    saved_freq = subject_get_int(cfg_cur.fg_freq);

    /* Con la app abierta el dial queda quieto: el barrido lo mueve por hardware y
     * el restore tiene que ser exacto. El volumen y los botones siguen andando. */
    main_screen_lock_freq(true);
    main_screen_lock_band(true);
    main_screen_lock_mode(true);

    /* El `dialog_style` trae el tamano, pero el objeto se queda en 0x0 hasta el
     * primer layout (medido en el banco: 0x0 al construir). Se fija el mismo
     * tamano/posicion del estilo (796x348 en 2,66) para que el marco y el fondo
     * del tema cubran el area desde el primer frame. */
    lv_obj_set_size(dialog.obj, 796, DIALOG_BOX_H);
    lv_obj_set_pos(dialog.obj, 2, 66);

    /* En el banco los assets del tema no estan en /dev/shm (/dev/shm vacio): la
     * imagen `dialog.bin` no carga y la home se ve a traves del panel. El fondo
     * propio (un rectangulo solido, sin depender del asset) deja el texto de la
     * app legible y se dibuja antes de los hijos. */
    lv_obj_add_event_cb(dialog.obj, draw_bg_cb, LV_EVENT_DRAW_MAIN_BEGIN, NULL);

    lbl_title = lv_label_create(dialog.obj);
    lv_obj_set_style_text_font(lbl_title, VF(28), 0);
    lv_obj_set_style_text_color(lbl_title, TC(text_head), 0);
    lv_label_set_text(lbl_title, "Propagation");
    lv_obj_set_pos(lbl_title, 18, 6);

    lbl_band = lv_label_create(dialog.obj);
    lv_obj_set_style_text_font(lbl_band, VF(22), 0);
    lv_obj_set_style_text_color(lbl_band, TC(text), 0);
    lv_obj_set_width(lbl_band, 760);
    lv_label_set_long_mode(lbl_band, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(lbl_band, 18, 40);

    lbl_step = lv_label_create(dialog.obj);
    lv_obj_set_style_text_font(lbl_step, VF(22), 0);
    lv_obj_set_style_text_color(lbl_step, TC(text_head), 0);
    lv_obj_set_pos(lbl_step, 18, 68);

    bar = lv_bar_create(dialog.obj);
    lv_obj_set_size(bar, 760, 10);
    lv_obj_set_pos(bar, 18, 96);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, TC(track), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, TC(ok_bar), LV_PART_INDICATOR);

    lbl_status = lv_label_create(dialog.obj);
    lv_obj_set_style_text_font(lbl_status, VF(20), 0);
    lv_obj_set_style_text_color(lbl_status, TC(text), 0);
    lv_obj_set_width(lbl_status, 760);
    lv_obj_set_height(lbl_status, 58);
    lv_label_set_long_mode(lbl_status, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(lbl_status, 18, 114);

    for (int i = 0; i < PROP_EVIDENCE_ROWS; i++) {
        lbl_evidence[i] = lv_label_create(dialog.obj);
        lv_obj_set_style_text_font(lbl_evidence[i], VF(18), 0);
        lv_obj_set_style_text_color(lbl_evidence[i], TC(text), 0);
        lv_obj_set_width(lbl_evidence[i], 760);
        lv_label_set_long_mode(lbl_evidence[i], LV_LABEL_LONG_DOT);
        lv_obj_set_pos(lbl_evidence[i], 18, 174 + i * 20);
        lv_label_set_text(lbl_evidence[i], "");
    }

    lbl_verdict = lv_label_create(dialog.obj);
    lv_obj_set_style_text_font(lbl_verdict, VF(26), 0);
    lv_obj_set_style_text_color(lbl_verdict, TC(text_head), 0);
    lv_obj_set_pos(lbl_verdict, 18, 278);
    lv_label_set_text(lbl_verdict, "");

    lbl_detail = lv_label_create(dialog.obj);
    lv_obj_set_style_text_font(lbl_detail, VF(18), 0);
    lv_obj_set_style_text_color(lbl_detail, TC(text_dim), 0);
    lv_obj_set_width(lbl_detail, 760);
    lv_obj_set_height(lbl_detail, 40);
    lv_label_set_long_mode(lbl_detail, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(lbl_detail, 18, 306);
    lv_label_set_text(lbl_detail, "");

    memset(&status, 0, sizeof(status));
    status.band       = propagation_ibp_band_of_freq((uint32_t) saved_freq);
    status.best        = -1;
    status.clock_synced = clock_time_synced();
    snprintf(status.band_name, sizeof(status.band_name), "%s", cfg_band_label_get());
    if (status.band_name[0] == '\0') {
        snprintf(status.band_name, sizeof(status.band_name), "current band");
    }

    /* El paso 1 muestra el plan desde que se abre: cuantos beacons aplican y
     * cuales son, con su frecuencia. */
    status.start_epoch = (int64_t) time(NULL);
    prepare_plan(status.start_epoch);

    render();

    if (params.propagation_run_on_open.x) {
        start_sweep();
    }
}

static void draw_bg_cb(lv_event_t *e) {
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    if (!ctx || !dialog.obj) {
        return;
    }

    lv_area_t coords;

    lv_obj_get_coords(dialog.obj, &coords);

    lv_draw_rect_dsc_t dsc;

    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color     = TC(panel);
    dsc.bg_opa       = LV_OPA_COVER;
    dsc.radius       = 0;
    dsc.border_width = 1;
    dsc.border_color = TC(dialog_border);
    dsc.border_opa   = 96;

    lv_draw_rect(ctx, &dsc, &coords);
}

static void destruct_cb(void) {
    /* Puede venir del marco de dialogos o del boton Cerrar: todo es idempotente.
     * Primero muere el hilo (puede estar sintonizando), despues se devuelve el VFO. */
    atomic_store(&stop_req, true);
    join_worker();

    if (tick_timer) {
        lv_timer_del(tick_timer);
        tick_timer = NULL;
    }

    radio_set_freq(saved_freq);

    main_screen_lock_band(false);
    main_screen_lock_mode(false);
    main_screen_lock_freq(false);

    ui_alive = false;
    lbl_title = lbl_band = lbl_step = lbl_status = lbl_verdict = lbl_detail = bar = NULL;

    for (int i = 0; i < PROP_EVIDENCE_ROWS; i++) {
        lbl_evidence[i] = NULL;
    }
}

static void ui_deleted_cb(lv_event_t *e) {
    (void) e;

    /* Mismo patron que dialog_cw_trainer (HALLAZGOS 37): si el objeto muere por
     * otro camino, el hilo y el timer tienen que morir con el. */
    if (ui_alive) {
        atomic_store(&stop_req, true);
        join_worker();
        if (tick_timer) {
            lv_timer_del(tick_timer);
            tick_timer = NULL;
        }
        radio_set_freq(saved_freq);
        main_screen_lock_band(false);
        main_screen_lock_mode(false);
        main_screen_lock_freq(false);
        ui_alive = false;
    }
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *) lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct(&dialog);
            break;

        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;

        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
    }
}

/* --- Render --------------------------------------------------------------- */

/* Potencia del escalon en decimas de W -> "100W", "10W", "1W" o "0.1W". */
static void power_text(int dw, char *buf, size_t size) {
    if (dw >= 10) {
        snprintf(buf, size, "%dW", dw / 10);
    } else {
        snprintf(buf, size, "0.%dW", dw);
    }
}

/* Nombre visible del veredicto (el del modulo puro es el token en ingles que
 * usa el control remoto). */
static const char *verdict_es(propagation_verdict_t v) {
    switch (v) {
        case PROPAGATION_VERDICT_OPEN:     return "ABIERTA";
        case PROPAGATION_VERDICT_MARGINAL: return "MARGINAL";
        case PROPAGATION_VERDICT_CLOSED:   return "CLOSED";
        case PROPAGATION_VERDICT_NO_DATA:  break;
    }

    return "NO DATA";
}

static void evidence_text(const prop_status_t *st, int i, char *buf, size_t size) {
    const prop_meas_t *m = &st->meas[i];
    const char *call;

    if (!m->done) {
        snprintf(buf, size, "... %s", propagation_ibp_call(st->plan[i]));
        lv_obj_set_style_text_color(lbl_evidence[i], TC(text_faint), 0);
        return;
    }

    call = propagation_ibp_call(m->beacon);
    if (!call) {
        call = "?";
    }

    char ladder[128];
    int  pos = 0;

    ladder[0] = '\0';

    for (int k = 0; k < PROPAGATION_LADDER_STEPS; k++) {
        int  dw = propagation_ladder_power_dw(k);
        char pw[16];

        power_text(dw, pw, sizeof(pw));

        if (!m->step_done[k]) {
            pos += snprintf(ladder + pos, sizeof(ladder) - (size_t) pos, " | ... %s", pw);
            continue;
        }

        bool hit = k < m->ladder;
        pos += snprintf(ladder + pos, sizeof(ladder) - (size_t) pos,
                        " | %s %s", hit ? "OK" : "NO", pw);
    }

    struct tm tm;
    time_t    t = (time_t) m->slot_start;

    gmtime_r(&t, &tm);

    snprintf(buf, size, "%s %u.%03u %02d:%02d:%02dZ | SNR %d%s%s",
             call,
             (unsigned) (propagation_ibp_freq(st->band) / 1000000),
             (unsigned) (propagation_ibp_freq(st->band) / 1000 % 1000),
             tm.tm_hour, tm.tm_min, tm.tm_sec, m->snr[0], ladder,
             m->ladder == 0 ? " | not heard" : "");

    lv_obj_set_style_text_color(lbl_evidence[i],
                                m->ladder > 0 ? TC(ok) : TC(text_dim), 0);
}

static void render(void) {
    if (!ui_alive || !lbl_title) {
        return;
    }

    prop_status_t st;

    status_copy(&st);

    int64_t now = (int64_t) time(NULL);
    int     beacon = -1;
    int64_t slot_start = 0;
    int     slot_left = 0;

    if (st.band >= 0) {
        propagation_slot(now, st.band, &beacon, &slot_start, &slot_left);
    }

    char buf[256];

    if (st.band >= 0 && st.plan_n > 0) {
        snprintf(buf, sizeof(buf), "Band %s  |  %d eligible IBP beacons  |  %d scheduled",
                 st.band_name, propagation_ibp_beacon_count(), st.plan_n);
    } else if (st.band < 0) {
        snprintf(buf, sizeof(buf), "Band %s  |  no IBP beacons", st.band_name);
    } else {
        snprintf(buf, sizeof(buf), "Band %s", st.band_name);
    }

    lv_label_set_text(lbl_band, buf);

    if (st.phase == PROP_PHASE_IDLE || st.phase == PROP_PHASE_PREPARE) {
        lv_label_set_text(lbl_step, "Step 1/4  |  Preparing");
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);

        if (st.band < 0) {
            snprintf(buf, sizeof(buf),
                     "No IBP beacons on %s: the IBP network starts at "
                     "14 MHz. Cluster spots appear below if available.", st.band_name);
        } else {
            snprintf(buf, sizeof(buf),
                     "Current band and IBP schedule ready.\n"
                     "NTP clock: %s.\nPress Run or Repeat to measure.",
                     st.clock_synced ? "synchronized" : "NOT synchronized (slot timing may drift)");
        }

        lv_label_set_text(lbl_status, buf);

        for (int i = 0; i < PROP_EVIDENCE_ROWS; i++) {
            if (i < st.plan_n) {
                const char *call = propagation_ibp_call(st.plan[i]);

                snprintf(buf, sizeof(buf), "... %s  %u.%03u MHz",
                         call ? call : "?",
                         (unsigned) (propagation_ibp_freq(st.band) / 1000000),
                         (unsigned) (propagation_ibp_freq(st.band) / 1000 % 1000));
                lv_label_set_text(lbl_evidence[i], buf);
                lv_obj_set_style_text_color(lbl_evidence[i], TC(text_faint), 0);
            } else {
                lv_label_set_text(lbl_evidence[i], "");
            }
        }

        lv_label_set_text(lbl_verdict, "");
        lv_label_set_text(lbl_detail, "");
    } else if (st.phase == PROP_PHASE_SYNC) {
        lv_label_set_text(lbl_step, "Step 2/4  |  Syncing");
        lv_bar_set_value(bar, 5, LV_ANIM_OFF);

        const char *call = (beacon >= 0) ? propagation_ibp_call(beacon) : NULL;

        snprintf(buf, sizeof(buf),
                 "NTP clock: %s\nBeacon on air: %s (%s, %d s left)\n%s",
                 st.clock_synced ? "OK: synchronized" : "NO: unsynchronized",
                 call ? call : "-", st.band_name, slot_left,
                 st.clock_synced ? "Slot aligned with radio clock."
                                 : "Beacon slot timing may drift without NTP.");
        lv_label_set_text(lbl_status, buf);
        lv_label_set_text(lbl_verdict, "");
        lv_label_set_text(lbl_detail, "");
    } else if (st.phase == PROP_PHASE_MEASURE) {
        lv_label_set_text(lbl_step, "Step 3/4  |  Measuring");

        int idx = st.plan_i;

        if (idx >= st.plan_n) {
            idx = st.plan_n - 1;
        }

        int     pct = st.plan_n > 0 ? (idx * 100) / st.plan_n : 0;
        int64_t slot = idx >= 0 ? st.plan_slot[idx] : 0;

        if (slot > 0 && now >= slot) {
            int elapsed = (int) (now - slot);

            if (elapsed > PROPAGATION_IBP_SLOT_S) {
                elapsed = PROPAGATION_IBP_SLOT_S;
            }

            pct += (elapsed * 100 / PROPAGATION_IBP_SLOT_S) / (st.plan_n > 0 ? st.plan_n : 1);
        }

        if (pct > 100) {
            pct = 100;
        }

        lv_bar_set_value(bar, pct, LV_ANIM_OFF);

        const char *call = idx >= 0 && idx < st.plan_n
                           ? propagation_ibp_call(st.plan[idx]) : NULL;
        int         wait = slot > now ? (int) (slot - now) : 0;

        snprintf(buf, sizeof(buf),
                 "Beacon %d of %d: %s at %u.%03u MHz\n%s\n%s",
                 idx + 1, st.plan_n, call ? call : "-",
                 (unsigned) (propagation_ibp_freq(st.band) / 1000000),
                 (unsigned) (propagation_ibp_freq(st.band) / 1000 % 1000),
                 wait > 0 ? "Waiting for slot..." : "Slot active: measuring power steps...",
                 wait > 0 ? "" : "100 W > 10 W > 1 W > 0.1 W");
        lv_label_set_text(lbl_status, buf);

        for (int i = 0; i < PROP_EVIDENCE_ROWS; i++) {
            char line[256];

            if (i >= st.plan_n) {
                lv_label_set_text(lbl_evidence[i], "");
                continue;
            }

            if (i < idx) {
                evidence_text(&st, i, line, sizeof(line));
                lv_label_set_text(lbl_evidence[i], line);
            } else if (i == idx) {
                evidence_text(&st, i, line, sizeof(line));
                lv_label_set_text(lbl_evidence[i], line);
            } else {
                lv_label_set_text(lbl_evidence[i], "");
            }
        }

        lv_label_set_text(lbl_verdict, "");
        lv_label_set_text(lbl_detail, "");
    } else if (st.phase == PROP_PHASE_RESULT || st.phase == PROP_PHASE_CANCELLED) {
        lv_label_set_text(lbl_step, st.phase == PROP_PHASE_CANCELLED
                                    ? "Step 4/4  |  Cancelled"
                                    : "Step 4/4  |  Result");
        lv_bar_set_value(bar, 100, LV_ANIM_OFF);

        lv_label_set_text(lbl_status, st.note);

        for (int i = 0; i < PROP_EVIDENCE_ROWS; i++) {
            char line[256];

            if (i >= st.plan_n) {
                lv_label_set_text(lbl_evidence[i], "");
                continue;
            }

            evidence_text(&st, i, line, sizeof(line));
            lv_label_set_text(lbl_evidence[i], line);
        }

        char verdict[32];

        if (st.band < 0) {
            snprintf(verdict, sizeof(verdict), "NO IBP BEACONS");
        } else {
            snprintf(verdict, sizeof(verdict), "%s", verdict_es(st.verdict));
        }

        lv_label_set_text(lbl_verdict, verdict);

        lv_color_t color = TC(text_head);

        if (st.verdict == PROPAGATION_VERDICT_OPEN) {
            color = TC(ok);
        } else if (st.verdict == PROPAGATION_VERDICT_MARGINAL) {
            color = TC(warn);
        } else if (st.verdict == PROPAGATION_VERDICT_CLOSED) {
            color = TC(err);
        }

        lv_obj_set_style_text_color(lbl_verdict, color, 0);

        /* Cobertura sobre los 18 beacons de la banda (no solo sobre los 5 que se
         * alcanzaron a medir) y, en 10 m, el tamano del registro IARU R2 que va
         * empaquetado en el firmware. */
        if (st.band == 4) {
            snprintf(buf, sizeof(buf),
                     "Measured %d/%d IBP beacons, %d heard. IARU R2 "
                     "(10 m): %d active of %d.",
                     st.measured, propagation_ibp_beacon_count(), st.heard,
                     propagation_iaru_active_count(), propagation_iaru_count());
        } else {
            snprintf(buf, sizeof(buf), "Measured %d/%d IBP beacons, %d heard.",
                     st.measured, propagation_ibp_beacon_count(), st.heard);
        }

        if (st.direction[0]) {
            char detail[512];

            snprintf(detail, sizeof(detail), "%s\n%s", buf, st.direction);
            lv_label_set_text(lbl_detail, detail);
        } else {
            lv_label_set_text(lbl_detail, buf);
        }

        char note[512];
        int  pos = 0;

        note[0] = '\0';

        if (st.cluster_n > 0) {
            pos += snprintf(note + pos, sizeof(note) - (size_t) pos, "%s\n", st.cluster[0]);
        }

        if (st.listening && st.listen_idx >= 0) {
            const char *call = propagation_ibp_call(st.listen_idx);

            pos += snprintf(note + pos, sizeof(note) - (size_t) pos,
                            "Listening to %s at %u.%03u MHz: Stop/Close restores VFO.\n",
                            call ? call : "el beacon",
                            (unsigned) (propagation_ibp_freq(st.band) / 1000000),
                            (unsigned) (propagation_ibp_freq(st.band) / 1000 % 1000));
        }

        snprintf(note + pos, sizeof(note) - (size_t) pos, "%s", st.note);
        lv_label_set_text(lbl_status, note);
    }

    /* El boton Run/Cancel y el de Escuchar reflejan el estado. */
    if (st.phase == PROP_PHASE_PREPARE || st.phase == PROP_PHASE_SYNC || st.phase == PROP_PHASE_MEASURE) {
        snprintf(run_label, sizeof(run_label), "Cancel");
    } else {
        snprintf(run_label, sizeof(run_label), "Run");
    }

    if (btn_run.label_obj) {
        button_update_label(&btn_run, run_label);
    }
}

/* --- Timer y despertar ---------------------------------------------------- */

static void ui_wake(void *arg) {
    (void) arg;

    if (!ui_alive) {
        return;
    }

    prop_phase_t phase;

    pthread_mutex_lock(&lock);
    phase = status.phase;
    pthread_mutex_unlock(&lock);

    if (phase == PROP_PHASE_RESULT || phase == PROP_PHASE_CANCELLED) {
        sweep_ui_end();
    }

    render();
}

static void tick_cb(lv_timer_t *t) {
    if (!ui_alive) {
        return;
    }

    prop_phase_t phase;

    pthread_mutex_lock(&lock);
    phase = status.phase;
    pthread_mutex_unlock(&lock);

    if (phase == PROP_PHASE_RESULT || phase == PROP_PHASE_CANCELLED) {
        sweep_ui_end();
    }

    render();

    (void) t;
}

/* --- Botones -------------------------------------------------------------- */

static void dialog_propagation_run_cb(button_item_t *item) {
    (void) item;

    prop_phase_t phase;

    pthread_mutex_lock(&lock);
    phase = status.phase;
    pthread_mutex_unlock(&lock);

    if (phase == PROP_PHASE_PREPARE || phase == PROP_PHASE_SYNC || phase == PROP_PHASE_MEASURE) {
        cancel_sweep();
    } else {
        start_sweep();
    }
}

static void dialog_propagation_repeat_cb(button_item_t *item) {
    (void) item;

    prop_phase_t phase;

    pthread_mutex_lock(&lock);
    phase = status.phase;
    pthread_mutex_unlock(&lock);

    if (phase == PROP_PHASE_PREPARE || phase == PROP_PHASE_SYNC || phase == PROP_PHASE_MEASURE) {
        return;
    }

    start_sweep();
}

static void dialog_propagation_listen_cb(button_item_t *item) {
    (void) item;

    prop_status_t st;

    status_copy(&st);

    if (st.listening) {
        listen_stop();
        render();
        return;
    }

    if (st.phase != PROP_PHASE_RESULT || st.best < 0 || st.meas[st.best].ladder < 1) {
        msg_update_text_fmt("Propagation: no heard beacon available to tune");
        return;
    }

    /* Escuchar es a proposito: deja el receptor en el beacon hasta que el
     * operador pare, repita o cierre. Cerrar SIEMPRE devuelve el VFO. */
    radio_set_freq((int32_t) propagation_ibp_freq(st.band));

    pthread_mutex_lock(&lock);
    status.listening = true;
    status.listen_idx = st.meas[st.best].beacon;
    pthread_mutex_unlock(&lock);

    button_update_label(&btn_listen, "Stop");
    render();
}

static void dialog_propagation_thresholds_cb(button_item_t *item) {
    int open = params.propagation_open_db.x;
    int marg = params.propagation_marginal_db.x;
    int idx  = propagation_snr_preset_index(open, marg);

    idx = (idx < 0) ? 0 : (idx + 1) % propagation_snr_preset_count();

    int new_open, new_marg;

    if (!propagation_snr_preset(idx, &new_open, &new_marg)) {
        return;
    }

    params_int32_set(&params.propagation_open_db, new_open);
    params_int32_set(&params.propagation_marginal_db, new_marg);
    params_flush();

    if (item) {
        buttons_refresh(item);
    }

    /* Si ya hay resultado, el veredicto se reinterpreta con los umbrales nuevos
     * sin volver a barrer (los SNRs medidos no cambian). */
    prop_status_t st;

    status_copy(&st);

    if (st.phase == PROP_PHASE_RESULT || st.phase == PROP_PHASE_CANCELLED) {
        pthread_mutex_lock(&lock);
        if (status.verdict_ready && status.band >= 0 && status.measured > 0) {
            status.verdict = propagation_verdict(status.best_snr, status.heard > 0,
                                                 new_open, new_marg);
        }
        pthread_mutex_unlock(&lock);
        render();
    }
}

static void dialog_propagation_close_cb(button_item_t *item) {
    (void) item;

    dialog_destruct(&dialog);
}

static const char *thresholds_label_fn(void) {
    /* Worst case for the two `%d` (11 chars each) plus "Thresholds:\n" and " dB"
     * is 38 bytes; 48 keeps `snprintf` out of truncation territory. */
    static char buf[48];

    snprintf(buf, sizeof(buf), "Thresholds:\n%d/%d dB",
             params.propagation_open_db.x, params.propagation_marginal_db.x);
    return buf;
}

/* --- Control remoto ------------------------------------------------------- */

bool dialog_propagation_remote_run(bool start) {
    if (!dialog_type_is_run(&dialog)) {
        return false;
    }

    prop_phase_t phase;

    pthread_mutex_lock(&lock);
    phase = status.phase;
    pthread_mutex_unlock(&lock);

    bool running = phase == PROP_PHASE_PREPARE || phase == PROP_PHASE_SYNC ||
                   phase == PROP_PHASE_MEASURE;

    if (start == running) {
        return true;
    }

    if (start) {
        start_sweep();
    } else {
        cancel_sweep();
    }

    return start != running;
}

void dialog_propagation_remote(const char *cmd, int32_t val, const char *text) {
    (void) text;

    if (!cmd || strcmp(cmd, "propagation")) {
        return;
    }

    dialog_propagation_remote_run(val != 0);
}

int dialog_propagation_remote_state(char *buf, size_t size) {
    prop_status_t st;

    status_copy(&st);

    const char *phase = "Idle";

    switch (st.phase) {
        case PROP_PHASE_PREPARE:   phase = "Preparing"; break;
        case PROP_PHASE_SYNC:      phase = "Syncing"; break;
        case PROP_PHASE_MEASURE:   phase = "Measuring"; break;
        case PROP_PHASE_RESULT:    phase = "Result"; break;
        case PROP_PHASE_CANCELLED: phase = "Cancelled"; break;
        case PROP_PHASE_IDLE:
        default:                   phase = "Idle"; break;
    }

    return snprintf(buf, size,
                    "{\"propagation_state\":\"%s\",\"propagation_band\":\"%s\","
                    "\"propagation_plan\":%d,\"propagation_done\":%d,"
                    "\"propagation_heard\":%d,\"propagation_snr\":%d,"
                    "\"propagation_ladder\":%d,\"propagation_listening\":%s}",
                    phase, st.band_name, st.plan_n, st.measured, st.heard,
                    st.best_snr, st.best >= 0 ? st.meas[st.best].ladder : 0,
                    st.listening ? "true" : "false");
}

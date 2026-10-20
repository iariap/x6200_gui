/*
 * Native test: the spectrum frame handoff between the producer thread and the
 * GUI thread (issue #149, "[LVGL8 OPT-3] Race-free spectrum snapshots").
 *
 * What the module promises (and this test pins):
 *
 *   1. The frame (800 samples + 800 peaks + the metadata the draw uses: grid,
 *      FFT width, filter edges, DNF, mode, TX flag) is published by the
 *      producer under data_mux and read by the draw as ONE snapshot: a draw
 *      never mixes two generations.
 *   2. The mutex is not held while the 800 lv_draw_line of the callback run:
 *      the producer (and any reader) makes progress while the draw is stopped
 *      in the middle of the trace.
 *   3. spectrum_clear() is synchronised with the same lock, so a clear
 *      concurrent with a draw also yields a single-generation frame.
 *   4. LVGL is only touched from the GUI thread: the producer defers the
 *      refresh through scheduler_put_noargs() and never invalidates.
 *   5. The refresh backlog is bounded (one queued refresh at a time, never
 *      blocking the producer) and a rejected enqueue is re-armed, so the
 *      spectrum cannot end up without refreshes.
 *   6. spectrum_data() cannot write past the frame (bounds), and the refresh
 *      reaches the scheduler even when the queue is full.
 *
 * How the interleaving is made deterministic: the stub stops the draw at an
 * exact lv_draw_line index (spectrum_stub_pause_at) and a helper thread runs the
 * producer action right there. No sleeps, no timing luck: the red run of the
 * pre-fix module is a mixed-generation frame (lines 0..k of generation A, the
 * rest of generation B) and a filter rectangle of the newer generation.
 *
 * RED evidence (base commit a69ea288, same test, same stub tree): the
 * "una generacion" cases fail with both generations counted in the same trace
 * and the metadata of the newer one, and the backlog case counts 50 queued
 * refreshes instead of 1. The commands are in the header of the run_native_tests.sh
 * block that builds this binary.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "spectrum.h"

#include "cfg/cfg.h"
#include "meter.h"
#include "params/params.h"
#include "radio.h"
#include "spectrum_stubs.h"
#include "test_framework.h"

/* Muestras del frame. Tiene que coincidir con SPECTRUM_SIZE (privado del
 * modulo) y con el ancho del widget del stub: si el modulo cambia de tamano,
 * el conteo de lineas dibujadas deja de dar 800 y el test lo delata. */
#define FRAME_N SPECTRUM_STUB_W

/* Generaciones del test: cada una es un valor constante en las 800 muestras.
 * Con la grilla del stub (S4..S9_20 = -103..-53, rango 50) y el widget de
 * 800x100 en (0,0), el dibujo deja la traza principal en y=90 (A), y=20 (B) y
 * y=100 (frame limpiado con S_MIN). */
#define GEN_A_DB    (-98.0f)
#define GEN_B_DB    (-63.0f)

/* Rectangulo del filtro por generacion: x1/x2 = 400 + 800*edge/100000. */
#define FILTER_A_FROM (-4000)
#define FILTER_A_TO   (4000)
#define FILTER_B_FROM (-1000)
#define FILTER_B_TO   (1000)
#define FILTER_A_X1   (400 + (800 * FILTER_A_FROM) / 100000)
#define FILTER_A_X2   (400 + (800 * FILTER_A_TO) / 100000)
#define FILTER_B_X1   (400 + (800 * FILTER_B_FROM) / 100000)
#define FILTER_B_X2   (400 + (800 * FILTER_B_TO) / 100000)

typedef enum {
    GEN_UNKNOWN = 0,
    GEN_A,
    GEN_B,
    GEN_CLEARED,
    GEN_MAX,
} gen_t;

static const char *gen_name(gen_t g) {
    switch (g) {
        case GEN_A:     return "A";
        case GEN_B:     return "B";
        case GEN_CLEARED: return "limpiado";
        case GEN_UNKNOWN: return "desconocido";
        case GEN_MAX:   break;
    }
    return "?";
}

static gen_t classify_y(int y) {
    if (y >= 96) {
        return GEN_CLEARED;
    }
    if (y >= 70) {
        return GEN_A;
    }
    if (y <= 40) {
        return GEN_B;
    }
    return GEN_UNKNOWN;
}

static void fill_generation(float *buf, float db) {
    for (unsigned i = 0; i < FRAME_N; i++) {
        buf[i] = db;
    }
}

/* Cuenta, en la traza principal del ultimo dibujo, cuantas lineas hay de cada
 * generacion. Solo mira las FRAME_N primeras lineas del color de la traza
 * principal: la linea central (que el callback dibuja despues, a y=0) queda
 * fuera. */
static unsigned scan_main_trace(unsigned counts[GEN_MAX]) {
    unsigned seen = 0;

    memset(counts, 0, sizeof(unsigned) * GEN_MAX);

    for (unsigned i = 0; i < spectrum_stub_lines_n && seen < FRAME_N; i++) {
        const spectrum_stub_line_t *l = &spectrum_stub_lines[i];

        if (l->color != SPECTRUM_STUB_COLOR_MAIN) {
            continue;
        }

        counts[classify_y(l->y1)]++;
        seen++;
    }

    return seen;
}

/* La generacion unica de la traza, o GEN_UNKNOWN si hay mezcla (o si no hay
 * traza completa). */
static gen_t single_generation(const unsigned counts[GEN_MAX], unsigned lines) {
    gen_t only = GEN_UNKNOWN;
    unsigned kinds = 0;

    if (lines != FRAME_N) {
        return GEN_UNKNOWN;
    }

    for (gen_t g = GEN_UNKNOWN; g < GEN_MAX; g++) {
        if (counts[g] > 0) {
            kinds++;
            only = g;
        }
    }

    if (kinds != 1 || only == GEN_UNKNOWN) {
        return GEN_UNKNOWN;
    }

    return only;
}

static void publish(float db, bool tx) {
    float buf[FRAME_N];

    fill_generation(buf, db);
    spectrum_data(buf, FRAME_N, tx);
}

/* Frame limpio + generacion A publicada: los picos arrancan en S_MIN, asi el
 * pico de A no queda contaminado por el de un caso anterior. */
static void reset_frame(void) {
    spectrum_clear();
    publish(GEN_A_DB, false);
}

/* Diagnostico de una traza mezclada: sale por stdout solo cuando hay mezcla (o
 * falta traza), asi la corrida roja dice que generacion aparecio en cada linea. */
static void report_mix(const unsigned counts[GEN_MAX], unsigned lines) {
    if (lines == FRAME_N && single_generation(counts, lines) != GEN_UNKNOWN) {
        return;
    }

    printf("traza mezclada: %u lineas principales, %s=%u %s=%u %s=%u %s=%u\n",
           lines,
           gen_name(GEN_A), counts[GEN_A], gen_name(GEN_B), counts[GEN_B],
           gen_name(GEN_CLEARED), counts[GEN_CLEARED],
           gen_name(GEN_UNKNOWN), counts[GEN_UNKNOWN]);
}

/* --- acciones del hilo productor, intercaladas en la pausa ---------------- */

typedef enum {
    ACT_NONE = 0,
    ACT_PUBLISH_B,
    ACT_CLEAR,
    ACT_PUBLISH_B_AND_FILTER,
    ACT_PUBLISH_B_CLEAR_AND_READ,
} action_t;

typedef struct {
    action_t action;
    bool     saw_pause;
    bool     done;
    double   elapsed_ms;
} job_t;

static job_t job;

static double now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1000000.0;
}

static void *helper_thread(void *arg) {
    job_t *j = arg;

    j->saw_pause = spectrum_stub_wait_paused(2000);

    if (!j->saw_pause) {
        j->done = true;
        spectrum_stub_resume();
        return NULL;
    }

    double t0 = now_ms();

    switch (j->action) {
        case ACT_PUBLISH_B:
            publish(GEN_B_DB, false);
            break;

        case ACT_CLEAR:
            spectrum_clear();
            break;

        case ACT_PUBLISH_B_AND_FILTER:
            publish(GEN_B_DB, false);
            spectrum_stub_subject_set(cfg_cur.filter.real.from, FILTER_B_FROM);
            spectrum_stub_subject_set(cfg_cur.filter.real.to, FILTER_B_TO);
            break;

        case ACT_PUBLISH_B_CLEAR_AND_READ:
            publish(GEN_B_DB, false);
            spectrum_clear();
            (void) spectrum_get_min();
            break;

        case ACT_NONE:
            break;
    }

    j->elapsed_ms = now_ms() - t0;
    j->done = true;

    spectrum_stub_resume();

    return NULL;
}

/* Dibuja el widget mientras un hilo aparte corre `action` en la pausa. */
static void draw_with_mid_action(int pause_at, action_t action) {
    pthread_t t;

    job.action = action;
    job.saw_pause = false;
    job.done = false;
    job.elapsed_ms = 0.0;

    spectrum_stub_pause_at(pause_at);

    CHECK(pthread_create(&t, NULL, helper_thread, &job) == 0);

    spectrum_stub_draw();

    CHECK(pthread_join(t, NULL) == 0);
    CHECK(job.saw_pause);
    CHECK(job.done);
    CHECK(!spectrum_stub_pause_timed_out());
}

/* --- 1. hilos: el productor no toca LVGL y el refresco va al hilo de GUI -- */

static void *producer_thread(void *arg) {
    (void) arg;

    publish(GEN_A_DB, false);

    return NULL;
}

static void test_producer_defers_lvgl_to_the_gui_thread(void) {
    spectrum_stub_reset();

    /* El hilo del test es el de LVGL; el productor corre en otro hilo, como
     * radio_thread() en la radio. */
    spectrum_stub_gui_thread();

    pthread_t t;

    CHECK(pthread_create(&t, NULL, producer_thread, NULL) == 0);
    CHECK(pthread_join(t, NULL) == 0);

    /* Nada de LVGL desde el hilo productor... */
    CHECK(spectrum_stub_foreign_lvgl == 0);
    CHECK(spectrum_stub_invalidates == 0);

    /* ...y el refresco quedo encolado para el hilo de GUI. */
    CHECK(spectrum_stub_sched_pending() == 1);

    spectrum_stub_sched_drain();

    CHECK(spectrum_stub_invalidates == 1);
    CHECK(spectrum_stub_foreign_lvgl == 0);
}

/* --- 2. snapshot: una sola generacion por dibujo --------------------------- */

static void test_draw_is_a_single_generation(void) {
    spectrum_stub_reset();

    /* Con picos activos el test cubre las dos trazas del frame. */
    params.spectrum_peak = true;

    reset_frame();
    spectrum_stub_subject_set(cfg_cur.filter.real.from, FILTER_A_FROM);
    spectrum_stub_subject_set(cfg_cur.filter.real.to, FILTER_A_TO);

    /* El productor publica la generacion B (y mueve el filtro) en pleno
     * dibujo: la traza ya dibujada es de A y lo que resta tambien tiene que
     * serlo. */
    draw_with_mid_action(700, ACT_PUBLISH_B_AND_FILTER);

    unsigned counts[GEN_MAX];
    unsigned lines = scan_main_trace(counts);

    report_mix(counts, lines);

    CHECK(lines == FRAME_N);
    CHECK(single_generation(counts, lines) == GEN_A);

    /* Los picos van por el mismo camino: 800 lineas del color del pico. */
    unsigned peak_lines = 0;
    unsigned peak_other = 0;

    for (unsigned i = 0; i < spectrum_stub_lines_n; i++) {
        const spectrum_stub_line_t *l = &spectrum_stub_lines[i];

        if (l->color != SPECTRUM_STUB_COLOR_PEAK) {
            continue;
        }

        peak_lines++;
        if (classify_y(l->y1) != GEN_A) {
            peak_other++;
        }
    }

    CHECK(peak_lines == FRAME_N);
    CHECK(peak_other == 0);

    /* Y los metadatos del frame dibujado son los de la misma generacion: el
     * rectangulo del filtro se lee despues del lazo de lineas. */
    CHECK(spectrum_stub_rects_n == 1);
    if (spectrum_stub_rects_n >= 1) {
        CHECK(spectrum_stub_rects[0].x1 == FILTER_A_X1);
        CHECK(spectrum_stub_rects[0].x2 == FILTER_A_X2);
    }

    params.spectrum_peak = false;
}

/* --- 3. spectrum_clear() concurrente -------------------------------------- */

static void test_clear_during_draw_is_not_mixed(void) {
    spectrum_stub_reset();

    reset_frame();
    spectrum_stub_subject_set(cfg_cur.filter.real.from, FILTER_A_FROM);
    spectrum_stub_subject_set(cfg_cur.filter.real.to, FILTER_A_TO);

    /* El reset del espectro (cambio de banda/VFO/FFT, todos en el hilo de GUI,
     * y el productor) cae en pleno dibujo. */
    draw_with_mid_action(700, ACT_CLEAR);

    unsigned counts[GEN_MAX];
    unsigned lines = scan_main_trace(counts);

    report_mix(counts, lines);

    CHECK(lines == FRAME_N);
    CHECK(single_generation(counts, lines) == GEN_A);

    CHECK(spectrum_stub_rects_n == 1);
    if (spectrum_stub_rects_n >= 1) {
        CHECK(spectrum_stub_rects[0].x1 == FILTER_A_X1);
        CHECK(spectrum_stub_rects[0].x2 == FILTER_A_X2);
    }
}

static void test_draw_after_clear_is_all_cleared(void) {
    spectrum_stub_reset();

    reset_frame();
    spectrum_clear();

    /* La foto se toma con el frame ya limpiado: la generacion B que llega a
     * mitad de dibujo no puede aparecer en esta traza. */
    draw_with_mid_action(700, ACT_PUBLISH_B);

    unsigned counts[GEN_MAX];
    unsigned lines = scan_main_trace(counts);

    report_mix(counts, lines);

    CHECK(lines == FRAME_N);
    CHECK(single_generation(counts, lines) == GEN_CLEARED);
}

/* --- 4. el mutex no se sostiene durante el dibujo ------------------------- */

static void test_producer_progresses_while_the_draw_is_paused(void) {
    spectrum_stub_reset();

    reset_frame();

    /* spectrum_data + spectrum_clear + spectrum_get_min tienen que completar
     * con el dibujo detenido en la linea 700: si el callback sostuviera el
     * mutex durante las 800 lv_draw_line, este hilo quedaria bloqueado, la
     * pausa venceria y el test lo veria. */
    draw_with_mid_action(700, ACT_PUBLISH_B_CLEAR_AND_READ);

    CHECK(job.done);
    CHECK(!spectrum_stub_pause_timed_out());

    /* Referencia del tiempo de lock: muy por debajo de los 2 s de la pausa. */
    CHECK(job.elapsed_ms < 500.0);
}

/* --- 5. backlog acotado y refresco que no se pierde ----------------------- */

static void test_refresh_backlog_is_bounded_and_never_lost(void) {
    spectrum_stub_reset();
    spectrum_stub_sched_drain();

    /* 50 frames sin drenar: el productor no acumula cuadros historicos. */
    for (int i = 0; i < 50; i++) {
        publish(GEN_A_DB, false);
    }

    if (spectrum_stub_sched_pending() != 1 || spectrum_stub_sched_peak() != 1) {
        printf("backlog: %u refrescos encolados (pico %u) para 50 frames\n",
               spectrum_stub_sched_pending(), spectrum_stub_sched_peak());
    }

    CHECK(spectrum_stub_sched_pending() == 1);
    CHECK(spectrum_stub_sched_peak() == 1);

    /* El refresco que quedo dibuja (no se perdio ninguno). */
    spectrum_stub_sched_drain();
    CHECK(spectrum_stub_invalidates == 1);

    /* El frame siguiente vuelve a encolar. */
    publish(GEN_A_DB, false);
    CHECK(spectrum_stub_sched_pending() == 1);
    spectrum_stub_sched_drain();

    /* Cola llena: el put se rechaza y el proximo frame lo reintenta (el
     * espectro no se queda sin refrescos). */
    spectrum_stub_sched_reject_next(1);
    publish(GEN_A_DB, false);
    CHECK(spectrum_stub_sched_pending() == 0);

    publish(GEN_A_DB, false);
    CHECK(spectrum_stub_sched_pending() == 1);

    spectrum_stub_sched_drain();
    CHECK(spectrum_stub_invalidates == 3);
}

/* --- 6. bounds ------------------------------------------------------------- */

static void test_oversized_frame_is_clamped(void) {
    spectrum_stub_reset();

    static float big[FRAME_N + 32];

    fill_generation(big, GEN_A_DB);

    /* El frame tiene FRAME_N muestras: un size mayor se recorta en vez de
     * escribir fuera del buffer (con ASan, la version sin recorte aborta aca). */
    spectrum_data(big, (uint16_t) (FRAME_N + 32), false);

    spectrum_stub_clear_records();
    spectrum_stub_draw();

    unsigned counts[GEN_MAX];
    unsigned lines = scan_main_trace(counts);

    report_mix(counts, lines);

    CHECK(lines == FRAME_N);
    CHECK(single_generation(counts, lines) == GEN_A);
}

/* --- 7. estres: productor, config y dibujo en paralelo --------------------- */

#define STRESS_DRAWS 120

static atomic_bool stress_stop;
static atomic_int  stress_bad_frames;
static atomic_int  stress_frames;

static void *stress_producer(void *arg) {
    (void) arg;

    while (!atomic_load(&stress_stop)) {
        publish(GEN_A_DB, false);
        publish(GEN_B_DB, false);
    }

    return NULL;
}

static void *stress_config(void *arg) {
    (void) arg;

    int32_t filter = FILTER_A_FROM;

    while (!atomic_load(&stress_stop)) {
        spectrum_clear();
        (void) spectrum_get_min();
        spectrum_update_min(GEN_B_DB);
        spectrum_update_max(GEN_A_DB);
        filter = (filter == FILTER_A_FROM) ? FILTER_B_FROM : FILTER_A_FROM;
        spectrum_stub_subject_set(cfg_cur.filter.real.from, filter);
    }

    return NULL;
}

static void *stress_gui(void *arg) {
    (void) arg;

    /* El dibujo de LVGL corre en ESTE hilo: es el hilo de GUI del estres. */
    spectrum_stub_gui_thread();

    for (int i = 0; i < STRESS_DRAWS; i++) {
        spectrum_stub_clear_records();
        spectrum_stub_draw();

        unsigned counts[GEN_MAX];
        unsigned lines = scan_main_trace(counts);

        atomic_fetch_add(&stress_frames, 1);

        if (single_generation(counts, lines) == GEN_UNKNOWN) {
            atomic_fetch_add(&stress_bad_frames, 1);
        }
    }

    atomic_store(&stress_stop, true);

    return NULL;
}

static void test_concurrent_stress_keeps_frames_consistent(void) {
    spectrum_stub_reset();

    atomic_store(&stress_stop, false);
    atomic_store(&stress_bad_frames, 0);
    atomic_store(&stress_frames, 0);

    pthread_t producer, config, gui;

    CHECK(pthread_create(&producer, NULL, stress_producer, NULL) == 0);
    CHECK(pthread_create(&config, NULL, stress_config, NULL) == 0);
    CHECK(pthread_create(&gui, NULL, stress_gui, NULL) == 0);

    CHECK(pthread_join(gui, NULL) == 0);
    atomic_store(&stress_stop, true);
    CHECK(pthread_join(producer, NULL) == 0);
    CHECK(pthread_join(config, NULL) == 0);

    if (atomic_load(&stress_bad_frames) > 0) {
        printf("estres: %d de %d dibujos con generaciones mezcladas\n",
               atomic_load(&stress_bad_frames), STRESS_DRAWS);
    }

    CHECK(atomic_load(&stress_frames) == STRESS_DRAWS);
    CHECK(atomic_load(&stress_bad_frames) == 0);
    CHECK(spectrum_stub_foreign_lvgl == 0);
}

/* --- 8. perfil (numeros para el reporte, no umbrales) --------------------- */

#define PROFILE_DRAWS 200

static atomic_bool profile_stop;
static double      profile_probe_max_ms;

static void *profile_probe(void *arg) {
    (void) arg;

    while (!atomic_load(&profile_stop)) {
        double t0 = now_ms();

        (void) spectrum_get_min();

        double dt = now_ms() - t0;

        if (dt > profile_probe_max_ms) {
            profile_probe_max_ms = dt;
        }
    }

    return NULL;
}

static void profile_draw_and_lock(void) {
    spectrum_stub_reset();

    reset_frame();

    atomic_store(&profile_stop, false);
    profile_probe_max_ms = 0.0;

    pthread_t probe;

    CHECK(pthread_create(&probe, NULL, profile_probe, NULL) == 0);

    double t0 = now_ms();

    for (int i = 0; i < PROFILE_DRAWS; i++) {
        spectrum_stub_clear_records();
        spectrum_stub_draw();
    }

    double draw_ms = (now_ms() - t0) / PROFILE_DRAWS;

    atomic_store(&profile_stop, true);
    CHECK(pthread_join(probe, NULL) == 0);

    double t1 = now_ms();

    for (int i = 0; i < PROFILE_DRAWS; i++) {
        publish(GEN_A_DB, false);
        spectrum_stub_sched_drain();
    }

    double data_ms = (now_ms() - t1) / PROFILE_DRAWS;

    printf("perfil host: dibujo %.1f us/dibujo; lock visto por spectrum_get_min "
           "%.1f us max; spectrum_data+refresco %.1f us/frame\n",
           draw_ms * 1000.0, profile_probe_max_ms * 1000.0, data_ms * 1000.0);
}

/* ==========================================================================
 * Geometria de la banda pasante RX a escala (issue #173, PR 1).
 *
 * spectrum_filter_band() es pura: se verifica el golden del contrato y los
 * limites. La posicion sale de los bordes reales de RF (from/to) y del ancho
 * mostrado (span), con x0 = w/2 y redondeo al entero mas cercano (en los medios,
 * alejandose del cero): por esa regla el contrato admite +/-1 px.
 * ========================================================================== */

static void check_band(const char *what, int32_t from, int32_t to, int32_t span, int32_t w,
                       bool visible, int32_t left, int32_t right) {
    spectrum_filter_band_t b = spectrum_filter_band(from, to, span, w);

    if (b.visible != visible || (visible && (b.left != left || b.right != right))) {
        printf("banda %s: from=%d to=%d span=%d w=%d -> visible=%d %d..%d (esperado %d %d..%d)\n",
               what, from, to, span, w, b.visible, b.left, b.right, visible, left, right);
    }

    CHECK(b.visible == visible);

    if (visible) {
        CHECK(b.left == left);
        CHECK(b.right == right);
        CHECK(b.left >= 0);
        CHECK(b.right <= w);
        CHECK(b.left <= b.right);
    }
}

/* Espejo por modo de src/cfg/mode.c::update_real_filters() (la unica fuente de
 * cfg_cur.filter.real.from/to): el test usa el mapeo DOCUMENTADO del firmware
 * para alimentar la geometria, no para reimplementarla. */
static void mode_real_edges(x6200_mode_t mode, int32_t low, int32_t high,
                            int32_t *from, int32_t *to) {
    switch (mode) {
        case x6200_mode_lsb:
        case x6200_mode_lsb_dig:
            *from = -high;
            *to   = -low;
            break;

        case x6200_mode_usb:
        case x6200_mode_usb_dig:
        case x6200_mode_cw:
        case x6200_mode_cwr:
            *from = low;
            *to   = high;
            break;

        case x6200_mode_am:
        case x6200_mode_sam:
        case x6200_mode_nfm:
        case x6200_mode_wfm:
            *from = -high;
            *to   = high;
            break;

        default:
            *from = low;
            *to   = high;
            break;
    }
}

static void check_mode_band(const char *what, x6200_mode_t mode, int32_t low, int32_t high,
                            int32_t span, int32_t w, int32_t left, int32_t right) {
    int32_t from, to;

    mode_real_edges(mode, low, high, &from, &to);
    check_band(what, from, to, span, w, true, left, right);
}

static void test_filter_band_geometry(void) {
    /* Golden del contrato (issue #173, seccion 2): USB-D, 14.200.000 Hz, span
     * 48.000 Hz, W=800. A la DERECHA del VFO, NO centrado. */
    check_band("golden USB-D", 225, 3525, 48000, 800, true, 404, 459);

    /* Espejo LSB-D: -3525..-225 -> x341..396. */
    check_band("golden LSB-D", -3525, -225, 48000, 800, true, 341, 396);

    /* El orden de from/to no cambia el resultado (min/max). */
    check_band("orden invertido", 3525, 225, 48000, 800, true, 404, 459);

    /* Mismo filtro, otros spans. */
    check_band("span 50 kHz", 225, 3525, 50000, 800, true, 404, 456);
    check_band("span 100 kHz", 225, 3525, 100000, 800, true, 402, 428);

    /* Redondeo: .5 alejandose del cero. span=1600, W=800 -> 0.5 px/Hz. */
    check_band("medio +", 1, 3, 1600, 800, true, 401, 402);
    check_band("medio -", -3, -1, 1600, 800, true, 398, 399);

    /* Division por cero / degenerados: nunca se dibuja. */
    check_band("span 0", 225, 3525, 0, 800, false, 0, 0);
    check_band("span negativo", 225, 3525, -48000, 800, false, 0, 0);
    check_band("w 0", 225, 3525, 48000, 0, false, 0, 0);

    /* Frecuencia fuera de vista: entera a la izquierda o a la derecha. */
    check_band("fuera izq", -100000, -90000, 48000, 800, false, 0, 0);
    check_band("fuera der", 90000, 100000, 48000, 800, false, 0, 0);

    /* Recortes parciales y totales. */
    check_band("recorte izq", -30000, -20000, 48000, 800, true, 0, 67);
    check_band("recorte der", 20000, 35000, 48000, 800, true, 733, 800);
    check_band("recorte total", -50000, 50000, 48000, 800, true, 0, 800);

    /* Zoom extremo (span chico): el filtro se sale por la derecha y se recorta. */
    check_band("zoom span 1 kHz", 225, 3525, 1000, 800, true, 580, 800);

    /* Enteros con signo en los extremos: sin overflow ni UB. */
    check_band("INT32_MIN", INT32_MIN, INT32_MIN + 1000, 48000, 800, false, 0, 0);
    check_band("INT32_MAX", INT32_MAX - 1000, INT32_MAX, 48000, 800, false, 0, 0);

    /* Espejos por modo sobre el mismo span del golden (48 kHz, W=800).
     * USB/LSB con el filtro de la captura del owner (low=225, high=3525). */
    check_mode_band("USB", x6200_mode_usb, 225, 3525, 48000, 800, 404, 459);
    check_mode_band("USB-D", x6200_mode_usb_dig, 225, 3525, 48000, 800, 404, 459);
    check_mode_band("LSB", x6200_mode_lsb, 225, 3525, 48000, 800, 341, 396);
    check_mode_band("LSB-D", x6200_mode_lsb_dig, 225, 3525, 48000, 800, 341, 396);

    /* AM/SAM/NFM/WFM: simetrico, LOW=0 desde la logica del firmware. */
    check_mode_band("AM", x6200_mode_am, 0, 4500, 48000, 800, 325, 475);
    check_mode_band("SAM", x6200_mode_sam, 0, 4500, 48000, 800, 325, 475);
    check_mode_band("NFM", x6200_mode_nfm, 0, 3500, 48000, 800, 342, 458);
    check_mode_band("WFM", x6200_mode_wfm, 0, 8000, 48000, 800, 267, 533);

    /* CW/CWR: signo del firmware, LOW negativo incluido. */
    check_mode_band("CW", x6200_mode_cw, -250, 250, 48000, 800, 396, 404);
    check_mode_band("CWR", x6200_mode_cwr, -250, 250, 48000, 800, 396, 404);
    check_mode_band("CW low negativo", x6200_mode_cw, -800, 200, 48000, 800, 387, 403);
}

/* El callback real dibuja la banda a escala: el relleno y las DOS lineas de
 * borde en los px del golden, dentro del area del espectro. */
static void test_filter_band_is_drawn_at_scale(void) {
    spectrum_stub_reset();
    spectrum_stub_gui_thread();

    reset_frame();
    spectrum_stub_subject_set(cfg_cur.fft_width, 48000);
    spectrum_stub_subject_set(cfg_cur.filter.real.from, 225);
    spectrum_stub_subject_set(cfg_cur.filter.real.to, 3525);
    spectrum_stub_subject_set(cfg_cur.filter.bw, 3300);

    spectrum_stub_clear_records();
    spectrum_stub_draw();

    /* Un relleno (rect) y dos lineas de borde, en x404..459, a la derecha del
     * centro (x=400) y NO centradas. */
    CHECK(spectrum_stub_rects_n == 1);
    if (spectrum_stub_rects_n == 1) {
        CHECK(spectrum_stub_rects[0].x1 == 404);
        CHECK(spectrum_stub_rects[0].x2 == 459);
    }

    int band_lines = 0;

    for (unsigned i = 0; i < spectrum_stub_lines_n; i++) {
        const spectrum_stub_line_t *l = &spectrum_stub_lines[i];

        if (l->color != SPECTRUM_STUB_COLOR_BAND) {
            continue;
        }

        band_lines++;
        CHECK(l->x1 == l->x2);                 /* verticales */
        CHECK(l->x1 == 404 || l->x1 == 459);
    }

    CHECK(band_lines == 2);

    /* El texto de BW real ("3.3k") entra en la banda y se dibuja entre las dos
     * lineas. */
    CHECK(spectrum_stub_labels_n == 1);
    if (spectrum_stub_labels_n == 1) {
        CHECK(spectrum_stub_labels[0].color == SPECTRUM_STUB_COLOR_BAND);
        CHECK(spectrum_stub_labels[0].x1 >= 404);
        CHECK(spectrum_stub_labels[0].x2 <= 459);
    }

    /* Los subjects son globales del stub: se restauran para no contaminar los
     * casos de #149 que corren despues con el span por defecto (100 kHz). */
    spectrum_stub_subject_set(cfg_cur.fft_width, 100000);
    spectrum_stub_subject_set(cfg_cur.filter.real.from, 0);
    spectrum_stub_subject_set(cfg_cur.filter.real.to, 3000);
    spectrum_stub_subject_set(cfg_cur.filter.bw, 3000);
}

/* Un filtro angosto (o fuera de vista) no dibuja ni relleno ni bordes. */
static void test_filter_band_hidden_when_out_of_view(void) {
    spectrum_stub_reset();
    spectrum_stub_gui_thread();

    reset_frame();
    spectrum_stub_subject_set(cfg_cur.fft_width, 48000);
    spectrum_stub_subject_set(cfg_cur.filter.real.from, -100000);
    spectrum_stub_subject_set(cfg_cur.filter.real.to, -90000);

    spectrum_stub_clear_records();
    spectrum_stub_draw();

    CHECK(spectrum_stub_rects_n == 0);

    for (unsigned i = 0; i < spectrum_stub_lines_n; i++) {
        CHECK(spectrum_stub_lines[i].color != SPECTRUM_STUB_COLOR_BAND);
    }

    spectrum_stub_subject_set(cfg_cur.fft_width, 100000);
    spectrum_stub_subject_set(cfg_cur.filter.real.from, 0);
    spectrum_stub_subject_set(cfg_cur.filter.real.to, 3000);
    spectrum_stub_subject_set(cfg_cur.filter.bw, 3000);
}

void test_spectrum(void) {
    /* spectrum_init() registra el callback de dibujo y arma los observers; los
     * casos corren despues, cada uno con el estado del stub limpio. */
    spectrum_stub_reset();
    CHECK(spectrum_init(NULL) != NULL);

    test_filter_band_geometry();
    test_filter_band_is_drawn_at_scale();
    test_filter_band_hidden_when_out_of_view();
    test_producer_defers_lvgl_to_the_gui_thread();
    test_draw_is_a_single_generation();
    test_clear_during_draw_is_not_mixed();
    test_draw_after_clear_is_all_cleared();
    test_producer_progresses_while_the_draw_is_paused();
    test_refresh_backlog_is_bounded_and_never_lost();
    test_oversized_frame_is_clamped();
    test_concurrent_stress_keeps_frames_consistent();
    profile_draw_and_lock();
}

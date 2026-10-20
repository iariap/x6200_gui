/*
 * Unit tests: adaptive tuning (issue #186).
 *
 * Contrato: velocidad real en detents/s, multiplicadores FIJOS por perfil,
 * umbrales derivados de T, histeresis 80%, freno a 180 ms, reverso x1, arranque
 * de dos intervalos, cola de 512 con backpressure y aplicacion int64 al VFO.
 *
 * Los tests viejos que esperaban 0,1x / 100x se retiraron con el motor anterior
 * (esos comportamientos quedan expresamente fuera del contrato).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "adaptive_tuning.h"
#include "rotary_queue.h"
#include "cb.h"
#include "test_framework.h"

#define T0_US       1000000ULL
#define TRACE_MAX   32

/* --- helpers ------------------------------------------------------------- */

/* Devuelve el multiplicador de cada detent de una traza con intervalo `dt_us`
 * (mismo signo). `out` recibe hasta `max` valores. */
static size_t run_trace(adaptive_tuning_state_t *st, adaptive_accel_t profile, uint8_t threshold,
                        uint64_t dt_us, int direction, size_t count, uint8_t *out, size_t max) {
    uint64_t ts = T0_US;

    for (size_t i = 0; i < count; i++) {
        uint8_t m = adaptive_tuning_detent(st, direction, ts, profile, threshold);

        if (out != NULL && i < max) {
            out[i] = m;
        }

        ts += dt_us;
    }

    return count;
}

static void check_trace(const uint8_t *got, const uint8_t *want, size_t n) {
    for (size_t i = 0; i < n; i++) {
        CHECK(got[i] == want[i]);

        if (got[i] != want[i]) {
            printf("    detent %zu: got x%u want x%u\n", i, got[i], want[i]);
            return;
        }
    }
}

/* --- 1. un detent exacto ------------------------------------------------- */

static void test_single_detent_exact(void) {
    static const int32_t steps[] = { 10, 100, 500, 1000, 5000 };
    static const adaptive_accel_t profiles[] = {
        ADAPTIVE_ACCEL_NONE, ADAPTIVE_ACCEL_LITE, ADAPTIVE_ACCEL_STRONG
    };

    for (size_t p = 0; p < sizeof(profiles) / sizeof(profiles[0]); p++) {
        for (size_t s = 0; s < sizeof(steps) / sizeof(steps[0]); s++) {
            for (int dir = -1; dir <= 1; dir += 2) {
                adaptive_tuning_state_t st;
                int32_t                 target = 0;

                adaptive_tuning_init(&st);

                uint8_t     m        = adaptive_tuning_detent(&st, dir, T0_US, profiles[p], 7);
                int64_t     weighted = (int64_t) dir * (int64_t) m;

                CHECK(m == 1);
                CHECK(adaptive_tuning_target_freq(14200003, weighted, steps[s], &target));
                CHECK(target == 14200003 + dir * steps[s]);
                CHECK(target != 14200010);          /* jamás 1 Hz ni grilla torcida */
            }
        }
    }
}

/* --- 2. curva: 5/10/20/40/70 detents/s, T=7 exacto 7/15/30/50 ------------ */

static void test_thresholds_t7(void) {
    uint8_t t[ADAPTIVE_LEVELS];

    adaptive_tuning_thresholds(7, t);
    CHECK(t[0] == 7);
    CHECK(t[1] == 15);
    CHECK(t[2] == 30);
    CHECK(t[3] == 50);
}

static void test_thresholds_t3_t15(void) {
    uint8_t t[ADAPTIVE_LEVELS];

    adaptive_tuning_thresholds(3, t);
    CHECK(t[0] == 3);
    CHECK(t[1] == 7);      /* ceil(45/7)  */
    CHECK(t[2] == 13);     /* ceil(90/7)  */
    CHECK(t[3] == 22);     /* ceil(150/7) */

    adaptive_tuning_thresholds(15, t);
    CHECK(t[0] == 15);
    CHECK(t[1] == 33);     /* ceil(225/7) */
    CHECK(t[2] == 65);     /* ceil(450/7) */
    CHECK(t[3] == 108);    /* ceil(750/7) */

    /* Fuera de rango: cae al default 7. */
    adaptive_tuning_thresholds(0, t);
    CHECK(t[0] == 7);
    adaptive_tuning_thresholds(2, t);
    CHECK(t[0] == 7);
    adaptive_tuning_thresholds(16, t);
    CHECK(t[0] == 7);
}

static void test_curve_lite_t7(void) {
    adaptive_tuning_state_t st;
    uint8_t                 m[TRACE_MAX];
    static const uint8_t    want_10[] = { 1, 2, 2, 2, 2, 2, 2, 2 };
    static const uint8_t    want_20[] = { 1, 2, 2, 3, 3, 3, 3, 3 };
    static const uint8_t    want_40[] = { 1, 2, 2, 6, 6, 6, 6, 6 };
    static const uint8_t    want_70[] = { 1, 2, 2, 10, 10, 10, 10, 10 };

    /* 5 detents/s = 200 ms entre detents: el freno (>=180 ms) lo deja en x1. */
    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_LITE, 7, 200000, +1, 8, m, TRACE_MAX);
    for (int i = 0; i < 8; i++) CHECK(m[i] == 1);

    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_LITE, 7, 100000, +1, 8, m, TRACE_MAX);
    check_trace(m, want_10, 8);

    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_LITE, 7, 50000, +1, 8, m, TRACE_MAX);
    check_trace(m, want_20, 8);

    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_LITE, 7, 25000, +1, 8, m, TRACE_MAX);
    check_trace(m, want_40, 8);

    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_LITE, 7, 14285, +1, 8, m, TRACE_MAX);
    check_trace(m, want_70, 8);
}

static void test_curve_strong_t7(void) {
    adaptive_tuning_state_t st;
    uint8_t                 m[TRACE_MAX];
    static const uint8_t    want_10[] = { 1, 3, 3, 3, 3, 3, 3, 3 };
    static const uint8_t    want_20[] = { 1, 3, 3, 6, 6, 6, 6, 6 };
    static const uint8_t    want_40[] = { 1, 3, 3, 12, 12, 12, 12, 12 };
    static const uint8_t    want_70[] = { 1, 3, 3, 24, 24, 24, 24, 24 };

    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_STRONG, 7, 100000, +1, 8, m, TRACE_MAX);
    check_trace(m, want_10, 8);

    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_STRONG, 7, 50000, +1, 8, m, TRACE_MAX);
    check_trace(m, want_20, 8);

    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_STRONG, 7, 25000, +1, 8, m, TRACE_MAX);
    check_trace(m, want_40, 8);

    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_STRONG, 7, 14285, +1, 8, m, TRACE_MAX);
    check_trace(m, want_70, 8);
}

static void test_none_ignores_speed_and_threshold(void) {
    adaptive_tuning_state_t st;

    adaptive_tuning_init(&st);

    for (int i = 0; i < 20; i++) {
        CHECK(adaptive_tuning_detent(&st, +1, T0_US + (uint64_t) i * 2000, ADAPTIVE_ACCEL_NONE, 3) == 1);
    }

    /* Cambiar T no lo despierta. */
    for (int i = 0; i < 20; i++) {
        CHECK(adaptive_tuning_detent(&st, +1, T0_US + 100000 + (uint64_t) i * 2000,
                                     ADAPTIVE_ACCEL_NONE, 15) == 1);
    }
}

/* T=3 acelera antes que T=15 con la MISMA traza (10 detents/s). */
static void test_threshold_changes_entry_point(void) {
    adaptive_tuning_state_t a, b;
    uint8_t                 ma[TRACE_MAX], mb[TRACE_MAX];

    adaptive_tuning_init(&a);
    adaptive_tuning_init(&b);

    run_trace(&a, ADAPTIVE_ACCEL_LITE, 3, 100000, +1, 8, ma, TRACE_MAX);
    run_trace(&b, ADAPTIVE_ACCEL_LITE, 15, 100000, +1, 8, mb, TRACE_MAX);

    CHECK(ma[7] == 3);      /* 10 dps >= T2=7 => nivel 2 */
    CHECK(mb[7] == 1);      /* 10 dps <  T1=15 => nivel 0 */
}

/* --- 3. histeresis del 80 % ---------------------------------------------- */

static void test_hysteresis_80(void) {
    adaptive_tuning_state_t st;
    uint64_t                ts = T0_US;

    adaptive_tuning_init(&st);

    /* Primer detent en T0; despues 9 intervalos de 14285 us = 70 dps. */
    adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7);

    for (int i = 1; i < 10; i++) {
        ts += 14285;
        adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7);
    }

    CHECK(st.level == 4);

    /* 45 dps >= 0.80 x 50 = 40: sigue en nivel 4. */
    ts += 22222;
    CHECK(adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7) == 10);
    CHECK(st.level == 4);

    /* 39 dps < 40: baja a nivel 3 (x6), pero 39 >= 0.80 x 30 = 24. */
    ts += 25641;
    CHECK(adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7) == 6);
    CHECK(st.level == 3);

    /* 23 dps < 24: baja a nivel 2 (x3); 23 >= 0.80 x 15 = 12. */
    ts += 43478;
    CHECK(adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7) == 3);
    CHECK(st.level == 2);

    /* 10 dps < 12: baja a nivel 1 (x2); 10 >= 0.80 x 7 = 5.6. */
    ts += 100000;
    CHECK(adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7) == 2);
    CHECK(st.level == 1);
}

/* --- 4. inverso ---------------------------------------------------------- */

static void test_reverse_first_detent_is_x1(void) {
    adaptive_tuning_state_t st;
    uint64_t                ts = T0_US;

    adaptive_tuning_init(&st);

    for (int i = 0; i < 8; i++) {
        adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_STRONG, 7);
        ts += 5000;
    }

    CHECK(st.level >= 3);

    /* Primera vuelta al otro lado: x1 y velocidad descartada. */
    CHECK(adaptive_tuning_detent(&st, -1, ts, ADAPTIVE_ACCEL_STRONG, 7) == 1);
    ts += 5000;

    /* Sigue acelerando solo si la nueva direccion persiste (arranque de dos
     * intervalos: nivel <= 1). */
    CHECK(adaptive_tuning_detent(&st, -1, ts, ADAPTIVE_ACCEL_STRONG, 7) == 3);

    /* Y la inversa otra vez. */
    CHECK(adaptive_tuning_detent(&st, +1, ts + 5000, ADAPTIVE_ACCEL_STRONG, 7) == 1);
}

/* --- 5. freno ------------------------------------------------------------ */

static void test_brake(void) {
    adaptive_tuning_state_t st;
    uint64_t                ts = T0_US;

    /* 180 ms exactos: x1 y sin aceleracion retenida. */
    adaptive_tuning_init(&st);
    adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7);

    for (int i = 1; i < 10; i++) {
        ts += 5000;
        adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7);
    }

    CHECK(st.level == 4);
    ts += 180000;
    CHECK(adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7) == 1);
    CHECK(st.level == 0);
    ts += 5000;
    CHECK(adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_LITE, 7) == 2);

    /* 250 y 400 ms: siempre x1, y la frecuencia no cambia sin detents (el motor
     * no tiene timers: solo devuelve valores cuando se le da un detent). */
    static const uint64_t slow[] = { 250000, 400000 };

    for (size_t i = 0; i < 2; i++) {
        adaptive_tuning_init(&st);
        ts = T0_US;
        adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_STRONG, 7);

        for (int k = 1; k < 10; k++) {
            ts += 14285;
            adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_STRONG, 7);
        }

        for (int k = 0; k < 6; k++) {
            ts += slow[i];
            CHECK(adaptive_tuning_detent(&st, +1, ts, ADAPTIVE_ACCEL_STRONG, 7) == 1);
        }
    }
}

/* --- 6. batches independientes bit a bit --------------------------------- */

typedef struct {
    int32_t raw;
    int64_t weighted;
    uint8_t last;
} seg_t;

/* Referencia event-by-event: es el mismo camino del consumidor (un evento EV_REL
 * -> weighted_event), pero sin cortes de lote. Devuelve la frecuencia final y
 * deja en `mults` el multiplicador de cada evento. */
static void reference_run(const int32_t *diffs, const uint64_t *ts, size_t n,
                          uint8_t *mults, int32_t *freq, int32_t step) {
    adaptive_tuning_state_t st;

    adaptive_tuning_init(&st);

    for (size_t i = 0; i < n; i++) {
        uint8_t mult = 1;
        int64_t weighted = adaptive_tuning_weighted_event(&st, diffs[i], ts[i],
                                                          ADAPTIVE_ACCEL_LITE, 7, &mult);
        int32_t target;

        mults[i] = mult;

        if (adaptive_tuning_target_freq(*freq, weighted, step, &target)) {
            *freq = target;
        }
    }
}

/* Mismo flujo, pero drenando/emitiendo en lotes de `per_call` eventos, como
 * hace la cola de 512 con la GUI. Compara contra la referencia. */
static size_t chunked_run(const int32_t *diffs, const uint64_t *ts, size_t n, size_t per_call,
                          uint8_t *mults, int32_t *freq, int32_t step,
                          seg_t *segs, size_t max_segs) {
    adaptive_tuning_state_t st;
    size_t                  segs_n = 0;

    adaptive_tuning_init(&st);

    for (size_t at = 0; at < n; at += per_call) {
        size_t  take = (n - at) > per_call ? per_call : (n - at);
        rotary_event_enhanced_t out[32];

        size_t got = adaptive_tuning_segments_ex(&st, diffs + at, ts + at, take,
                                                 ADAPTIVE_ACCEL_LITE, 7, out,
                                                 sizeof(out) / sizeof(out[0]),
                                                 mults + at, take);

        for (size_t i = 0; i < got && segs_n < max_segs; i++) {
            segs[segs_n].raw      = out[i].raw_diff;
            segs[segs_n].weighted = out[i].weighted_detents;
            segs[segs_n].last     = out[i].last_multiplier;
            segs_n++;

            int32_t target;

            if (adaptive_tuning_target_freq(*freq, out[i].weighted_detents, step, &target)) {
                *freq = target;
            }
        }
    }

    return segs_n;
}

static void test_batches_are_bit_identical(void) {
    enum { N = 20 };
    int32_t  diffs[N];
    uint64_t ts[N];
    uint64_t t = T0_US;

    /* Traza fija: 8 rapidos +, 4 lentos +, 5 rapidos -, 3 lentos -. */
    for (int i = 0; i < N; i++) {
        if (i < 8)       { diffs[i] = +1; t += 10000; }
        else if (i < 12) { diffs[i] = +1; t += 250000; }
        else if (i < 17) { diffs[i] = -1; t += 8000; }
        else             { diffs[i] = -1; t += 300000; }

        ts[i] = t;
    }

    uint8_t mref[N], m1[N], m4[N], mall[N];
    seg_t   one[32], four[32], all[32];
    int32_t fref = 14200003, f1 = 14200003, f4 = 14200003, fa = 14200003;

    reference_run(diffs, ts, N, mref, &fref, 10);

    size_t n1 = chunked_run(diffs, ts, N, 1, m1, &f1, 10, one, 32);
    size_t n4 = chunked_run(diffs, ts, N, 4, m4, &f4, 10, four, 32);
    size_t na = chunked_run(diffs, ts, N, N, mall, &fa, 10, all, 32);

    /* Lista de multiplicadores y frecuencia final: bit a bit, sin importar el
     * tamano de lote. */
    for (size_t i = 0; i < N; i++) {
        CHECK(mref[i] == m1[i]);
        CHECK(mref[i] == m4[i]);
        CHECK(mref[i] == mall[i]);
    }

    CHECK(fref == f1);
    CHECK(fref == f4);
    CHECK(fref == fa);
    CHECK(n1 > 0);
    CHECK(n4 > 0);
    CHECK(na > 0);

    /* La reversa tiene que llegar como tramos separados: hay al menos un tramo
     * positivo y uno negativo (nunca un neto de signos contrarios). */
    bool positive = false, negative = false;

    for (size_t i = 0; i < n1; i++) {
        if (one[i].raw > 0) positive = true;
        if (one[i].raw < 0) negative = true;
    }

    CHECK(positive && negative);

    /* La suma cruda de los tramos conserva todos los eventos (sin perder ni
     * duplicar); cada tramo ya sale con un solo signo de adaptive_tuning_segments(). */
    int64_t raw_total = 0;

    for (size_t i = 0; i < n1; i++) {
        CHECK(one[i].raw != 0);
        raw_total += one[i].raw;
    }

    CHECK(raw_total == 4);      /* 12 detents + y 8 -, sin perder ninguno */
}

/* --- 7. cola: overflow 512+, concurrente, sin perdida ni deadlock --------- */

typedef struct {
    rotary_queue_t *q;
    int             count;
} producer_arg_t;

static void *producer_thread(void *arg) {
    producer_arg_t *p = (producer_arg_t *) arg;

    for (int i = 0; i < p->count; i++) {
        /* timestamp creciente y diff +1: el consumidor puede reconstruir el
         * orden exacto (i) sin ambiguedad. */
        rotary_queue_push(p->q, +1, (uint64_t) (i + 1) * 10000ULL);
    }

    return NULL;
}

static void test_queue_overflow_backpressure(void) {
    rotary_queue_t  q;
    producer_arg_t  pa;
    pthread_t       th;
    rotary_data_t   d;
    int             received = 0;
    int             last     = -1;
    bool            order_ok = true;
    uint32_t        high = 0, waits = 0;

    rotary_queue_init(&q);

    pa.q     = &q;
    pa.count = 2000;

    CHECK(pthread_create(&th, NULL, producer_thread, &pa) == 0);

    /* El consumidor empieza despues: la cola se llena de verdad y el productor
     * tiene que esperar (backpressure), no descartar. */
    usleep(20000);

    while (received < pa.count) {
        if (!rotary_queue_pop(&q, &d)) {
            /* el productor esta por delante; se le da aire */
            usleep(1000);
            continue;
        }

        int index = (int) (d.timestamp_us / 10000ULL) - 1;

        if (index != last + 1 || d.diff != 1) {
            order_ok = false;
        }

        last = index;
        received++;
    }

    CHECK(pthread_join(th, NULL) == 0);

    rotary_queue_stats(&q, &high, &waits);

    CHECK(order_ok);
    CHECK(received == 2000);            /* ninguno omitido ni duplicado */
    CHECK(high == ROTARY_QUEUE_SIZE);   /* se lleno (512) */
    CHECK(waits > 0);                   /* el productor espero espacio */
    CHECK(rotary_queue_count(&q) == 0);
}

/* --- 8. no cruce VOL/MFK <-> VFO ----------------------------------------- */

static void test_no_cross_between_encoders(void) {
    /* El motor del dial solo cambia con SUS detents: actividad de otro encoder
     * (que en el firmware va por teclas y no toca el motor) no lo altera. */
    adaptive_tuning_state_t vfo, otro;

    adaptive_tuning_init(&vfo);
    adaptive_tuning_init(&otro);

    for (int i = 0; i < 10; i++) {
        adaptive_tuning_detent(&vfo, +1, T0_US + (uint64_t) i * 5000, ADAPTIVE_ACCEL_LITE, 7);
    }

    uint8_t level_before = vfo.level;

    /* El otro encoder "gira" muchisimo. */
    for (int i = 0; i < 1000; i++) {
        adaptive_tuning_detent(&otro, -1, T0_US + (uint64_t) i * 5000, ADAPTIVE_ACCEL_STRONG, 7);
    }

    CHECK(vfo.level == level_before);
    CHECK(otro.level >= 1);

    /* Y el motor del VFO sigue exactamente donde estaba. */
    CHECK(adaptive_tuning_detent(&vfo, +1, T0_US + 50000, ADAPTIVE_ACCEL_LITE, 7) == 10);
}

/* --- 9. grilla y limites int32 ------------------------------------------- */

static void test_grid_and_limits(void) {
    int32_t target = 0;

    /* Caso del contrato: 14.200.003 + un detent x1 de 10 Hz => 14.200.013. */
    CHECK(adaptive_tuning_target_freq(14200003, 1, 10, &target));
    CHECK(target == 14200013);
    CHECK(target != 14200010);          /* align_int() habria redondeado a la grilla */
    CHECK(target != 14200000);

    /* Desplazamientos negativos y multiplicadores grandes. */
    CHECK(adaptive_tuning_target_freq(14200013, -(int64_t) 24, 5000, &target));
    CHECK(target == 14200013 - 120000);

    /* Desbordes: no se escribe `out`. */
    target = 12345;
    CHECK(!adaptive_tuning_target_freq(INT32_MAX, 1, 1, &target));
    CHECK(target == 12345);
    CHECK(!adaptive_tuning_target_freq(INT32_MIN, -1, 1, &target));
    CHECK(target == 12345);
    CHECK(!adaptive_tuning_target_freq(0, (int64_t) 1 << 40, 5000, &target));
    CHECK(target == 12345);

    /* Cerca del limite pero valido. */
    CHECK(adaptive_tuning_target_freq(INT32_MAX - 10, 1, 10, &target));
    CHECK(target == INT32_MAX);
}

/* --- 10. CB: 1 detent = 1 canal, tambien con Strong y alta velocidad ------ */

static void test_cb_one_detent_one_channel(void) {
    int32_t ch1 = cb_freq_of_channel(CB_PLAN_C, 1);
    int32_t ch23 = cb_freq_of_channel(CB_PLAN_C, 23);
    int32_t ch40 = cb_freq_of_channel(CB_PLAN_C, 40);

    CHECK(cb_step_channel(CB_PLAN_C, ch1, +1) == 2);
    CHECK(cb_step_channel(CB_PLAN_C, ch23, +1) == 24);

    /* 23 -> 24 BAJA de frecuencia aunque el numero de canal suba. */
    CHECK(cb_freq_of_channel(CB_PLAN_C, 24) < ch23);

    CHECK(cb_step_channel(CB_PLAN_C, ch40, +1) == 40);   /* saturado */
    CHECK(cb_step_channel(CB_PLAN_C, ch1, -1) == 1);

    /* Fuera de canal: engancha el primero en la direccion del giro. */
    int32_t off = ch1 + 5;

    CHECK(cb_step_channel(CB_PLAN_C, off, +1) == 2);
    CHECK(cb_step_channel(CB_PLAN_C, off, -1) == 1);

    /* El receptor usa raw_diff: con Strong y nivel alto, un detent sigue siendo
     * un canal (1 -> 2), no un salto en Hz. */
    adaptive_tuning_state_t st;
    uint8_t m[16];

    adaptive_tuning_init(&st);
    run_trace(&st, ADAPTIVE_ACCEL_STRONG, 7, 5000, +1, 8, m, 16);
    CHECK(m[7] == 24);   /* el motor acelera... */

    /* ...pero el segmento trae raw_diff = 1 y CB lo usa tal cual. */
    int32_t diffs[16];
    uint64_t ts[16];
    for (int i = 0; i < 16; i++) { diffs[i] = +1; ts[i] = T0_US + (uint64_t) i * 5000; }

    rotary_event_enhanced_t segs[4];
    size_t n = adaptive_tuning_segments(&st, diffs, ts, 16, ADAPTIVE_ACCEL_STRONG, 7, segs, 4);

    CHECK(n == 1);
    CHECK(segs[0].raw_diff == 16);
    /* 16 detents en CB => 16 canales desde el 1, saturado en 40. */
    CHECK(cb_step_channel(CB_PLAN_C, ch1, segs[0].raw_diff) == 17);
}

/* --- 11. saneamiento del parametro --------------------------------------- */

static void test_threshold_sanitize(void) {
    CHECK(adaptive_tuning_threshold_sanitize(0) == 7);
    CHECK(adaptive_tuning_threshold_sanitize(2) == 7);
    CHECK(adaptive_tuning_threshold_sanitize(3) == 3);
    CHECK(adaptive_tuning_threshold_sanitize(7) == 7);
    CHECK(adaptive_tuning_threshold_sanitize(15) == 15);
    CHECK(adaptive_tuning_threshold_sanitize(16) == 7);
    CHECK(adaptive_tuning_threshold_sanitize(255) == 7);
}

/* --- 12. EV_REL con |diff| > 1: se divide interpolando tiempos ------------- */

static void test_single_event_split_is_x1(void) {
    adaptive_tuning_state_t st;

    adaptive_tuning_init(&st);

    /* Primer evento con diff 4: no hay timestamp anterior => todos x1. */
    CHECK(adaptive_tuning_weighted_event(&st, +4, T0_US, ADAPTIVE_ACCEL_STRONG, 7, NULL) == 4);

    /* Evento siguiente de 4 detents en 20 ms: se interpola y acelera. */
    int64_t w = adaptive_tuning_weighted_event(&st, +4, T0_US + 20000, ADAPTIVE_ACCEL_STRONG, 7, NULL);

    CHECK(w > 4);
}

void test_adaptive_tuning(void) {
    test_single_detent_exact();
    test_thresholds_t7();
    test_thresholds_t3_t15();
    test_curve_lite_t7();
    test_curve_strong_t7();
    test_none_ignores_speed_and_threshold();
    test_threshold_changes_entry_point();
    test_hysteresis_80();
    test_reverse_first_detent_is_x1();
    test_brake();
    test_batches_are_bit_identical();
    test_queue_overflow_backpressure();
    test_no_cross_between_encoders();
    test_grid_and_limits();
    test_cb_one_detent_one_channel();
    test_threshold_sanitize();
    test_single_event_split_is_x1();
}

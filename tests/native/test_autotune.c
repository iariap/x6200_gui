/*
 * Unit tests: the CW/CWR fine-tuning assistant core (issue #190, "Auto Tune
 * v2") - sample gate, robust statistics, the single correction, the two phases,
 * the freshness/timing contract and the dial-ownership rule. No LVGL, no radio,
 * no liquid-dsp: fake clock, fake samples, and a tiny rig that drives the core
 * exactly as src/autotune_ui.c does (see `rig_tick`).
 *
 * The sign of the error (`error_hz > 0` = tone ABOVE the pitch) is defined by the
 * DSP layer; the FFT/real-signal proof of that convention is
 * tests/native/test_cw_tune_sign.c (needs liquid-dsp, the same FFT as
 * src/cw.cpp).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "autotune.h"
#include "test_framework.h"

#define START       14074000
#define BAND_START  14070000u
#define BAND_STOP   14350000u
#define T0          100000ull      /* reloj falso, ms (get_time() arranca > 0) */

/* El nucleo trata el modo como un entero opaco: solo lo compara. */
#define CTX_MODE_CW   1
#define CTX_MODE_CWR  2

/* --- un trama como la publica el hilo del DSP (src/cw.cpp) ---------------- */

static autotune_sample_t dsp_sample(uint64_t seq, uint64_t ts, float error,
                                    float ratio, float offset) {
    autotune_sample_t s;

    s.sequence = seq;
    s.timestamp_ms = ts;
    s.error_hz = error;
    s.ratio = ratio;
    s.dds_offset_hz = offset;
    /* La validez la calcula el DSP: marca + SNR + finitud + rango del estimador. */
    s.valid = (ratio >= AUTOTUNE_MIN_SNR_RATIO) && isfinite(error) &&
              (error >= -AUTOTUNE_SAMPLE_ERROR_LIMIT_HZ) &&
              (error <= AUTOTUNE_SAMPLE_ERROR_LIMIT_HZ);
    return s;
}

static autotune_context_t ctx_of(int32_t mode, int32_t band, int32_t vfo, int32_t pitch,
                                 int32_t freq) {
    autotune_context_t ctx;

    ctx.mode = mode;
    ctx.band_id = band;
    ctx.vfo = vfo;
    ctx.pitch = pitch;
    ctx.freq = freq;
    ctx.rx = true;
    ctx.locked = false;
    ctx.rit = false;
    return ctx;
}

/* --- banco minimo: nucleo + dial falso + reloj falso ---------------------- */

typedef enum {
    RIG_NONE = 0,
    RIG_RUNNING,
    RIG_TUNED,
    RIG_FAIL,
    RIG_CANCELLED,
} rig_outcome_t;

typedef struct {
    autotune_t         at;
    autotune_context_t ctx;
    int32_t            dial;
    float              tone_error;   /* lo que mediria la FFT con el dial actual */
    float              dds_offset;
    uint64_t           now;
    uint64_t           seq;
    int32_t            writes;
    int32_t            last_write;
    rig_outcome_t      outcome;
    uint32_t           band_start;
    uint32_t           band_stop;
} rig_t;

static void rig_sync(rig_t *r) {
    r->ctx.freq = r->dial;
}

/* Mueve el dial como lo haria Auto Tune y actualiza la fisica del tono: en CW
 * subir el dial BAJA el tono de audio; en CWR (banda lateral invertida) subirlo
 * lo SUBE. Es lo que permite comprobar que la correccion converge a error ~= 0. */
static void rig_dial_set(rig_t *r, int32_t target) {
    int32_t d = target - r->dial;

    r->tone_error += (r->at.mode == AUTOTUNE_MODE_CW) ? -(float) d : (float) d;
    r->dial = target;
    r->writes++;
    r->last_write = target;
}

static void rig_end(rig_t *r, bool restore) {
    if (restore && r->at.wrote) {
        rig_sync(r);
        if (autotune_owns(&r->at, &r->ctx)) {
            autotune_note_rollback(&r->at);
            rig_dial_set(r, r->at.start_freq);
        }
    }
    autotune_reset(&r->at);
    if (r->outcome == RIG_RUNNING) {
        r->outcome = RIG_FAIL;
    }
}

/* Un tick del coordinador (espeja src/autotune_ui.c::tick_cb). `s` puede ser
 * NULL: es el caso "vencio el reloj, no llego trama". */
static void rig_tick(rig_t *r, const autotune_sample_t *s) {
    if (r->at.state == AUTOTUNE_STATE_IDLE) {
        return;
    }

    rig_sync(r);

    if (r->ctx.rit || autotune_context_broken(&r->at, &r->ctx)) {
        r->outcome = RIG_CANCELLED;
        rig_end(r, false);
        return;
    }
    if (autotune_session_timeout(&r->at, r->now)) {
        r->outcome = RIG_FAIL;
        rig_end(r, true);
        return;
    }
    if (s == NULL) {
        if (autotune_phase_timeout(&r->at, r->now) &&
            r->at.state != AUTOTUNE_STATE_IDLE) {
            r->outcome = RIG_FAIL;
            rig_end(r, true);
        }
        return;
    }
    if (autotune_take(&r->at, s, r->now) != AUTOTUNE_TAKE_READY) {
        if (autotune_phase_timeout(&r->at, r->now)) {
            r->outcome = RIG_FAIL;
            rig_end(r, true);
        }
        return;
    }

    if (r->at.state == AUTOTUNE_STATE_COLLECT) {
        int32_t           target = 0;
        autotune_result_t result =
            autotune_collect_result(&r->at, r->band_start, r->band_stop, &target);

        if (result == AUTOTUNE_RESULT_TUNED) {
            r->outcome = RIG_TUNED;
            rig_end(r, false);
            return;
        }
        if (result == AUTOTUNE_RESULT_NO_ADJUST) {
            r->outcome = RIG_FAIL;
            rig_end(r, false);
            return;
        }
        rig_sync(r);
        if (!autotune_owns(&r->at, &r->ctx)) {
            r->outcome = RIG_FAIL;
            rig_end(r, false);
            return;
        }
        autotune_note_write(&r->at, target);
        rig_dial_set(r, target);
        autotune_verify_start(&r->at, r->now, s->sequence, r->now);
        return;
    }

    if (autotune_verify_result(&r->at) == AUTOTUNE_RESULT_TUNED) {
        r->outcome = RIG_TUNED;
        rig_end(r, false);
        return;
    }
    r->outcome = RIG_FAIL;
    rig_end(r, true);
}

static void rig_start(rig_t *r, autotune_mode_t mode, int32_t freq, float tone_error) {
    autotune_context_t ctx = ctx_of(mode == AUTOTUNE_MODE_CWR ? CTX_MODE_CWR : CTX_MODE_CW,
                                    7, 0, 700, freq);

    memset(r, 0, sizeof(*r));
    r->ctx = ctx;
    r->dial = freq;
    r->tone_error = tone_error;
    r->dds_offset = 0.0f;
    r->now = T0;
    r->seq = 40;                 /* el DSP ya venia publicando tramas */
    r->outcome = RIG_RUNNING;
    r->band_start = BAND_START;
    r->band_stop = BAND_STOP;
    autotune_collect_start(&r->at, mode, &ctx, r->now, r->seq);
    r->seq++;                    /* la primera trama aceptada es POSTERIOR */
}

/* Una trama "normal": la FFT mide `error` respecto del pitch, ya compensado el
 * offset del DDS (el hilo del DSP lo hace; el nucleo lo recibe normalizado). El
 * banco no fabrica tramas DENTRO de la guarda de 300 ms posterior a una
 * escritura: salta a la primera que el nucleo puede aceptar (que las de adentro
 * se descarten tiene su propio test). */
static void rig_normal_frame(rig_t *r, float error, float ratio) {
    uint64_t ts = r->now + 185;

    if (r->at.state == AUTOTUNE_STATE_VERIFY && ts < r->at.min_ms) {
        ts = r->at.min_ms;
    }
    r->now = ts;

    autotune_sample_t s = dsp_sample(r->seq, ts, error, ratio, r->dds_offset);

    r->seq++;
    rig_tick(r, &s);
}

static void rig_idle(rig_t *r, uint64_t dt_ms) {
    r->now += dt_ms;
    rig_tick(r, NULL);
}

/* --- 1. gate de la trama --------------------------------------------------- */

static void test_sample_gate(void) {
    autotune_sample_t s = dsp_sample(1, 1000, 0.0f, 20.0f, 0.0f);

    CHECK(autotune_sample_ok(&s));
    CHECK(!autotune_sample_ok(NULL));

    s.valid = false;                      /* el DSP ya la descarto */
    CHECK(!autotune_sample_ok(&s));

    s = dsp_sample(1, 1000, NAN, 20.0f, 0.0f);
    CHECK(!autotune_sample_ok(&s));

    s = dsp_sample(1, 1000, INFINITY, 20.0f, 0.0f);
    CHECK(!autotune_sample_ok(&s));

    s = dsp_sample(1, 1000, 0.0f, INFINITY, 0.0f);
    CHECK(!autotune_sample_ok(&s));

    /* divisor de ruido = 0 -> relacion 0 (no es una marca) */
    s = dsp_sample(1, 1000, 0.0f, 0.0f, 0.0f);
    CHECK(!autotune_sample_ok(&s));

    /* justo por debajo del minimo (4.0 = 6 dB de potencia) */
    s = dsp_sample(1, 1000, 0.0f, 3.99f, 0.0f);
    CHECK(!autotune_sample_ok(&s));
    s = dsp_sample(1, 1000, 0.0f, 4.0f, 0.0f);
    CHECK(autotune_sample_ok(&s));

    /* fuera del rango del estimador (overflow incluido) */
    s = dsp_sample(1, 1000, 400.0f, 20.0f, 0.0f);
    CHECK(!autotune_sample_ok(&s));
    s = dsp_sample(1, 1000, 1.0e30f, 20.0f, 0.0f);
    CHECK(!autotune_sample_ok(&s));
}

/* --- 2. estadistica robusta ----------------------------------------------- */

static void test_median_robust_to_outlier(void) {
    const float a[] = { 10.0f, 12.0f, 500.0f };   /* el atipico no mueve la mediana */
    const float b[] = { 12.0f, 9.0f, 15.0f };
    const float c[] = { 7.0f, 9.0f };             /* par: promedio de los dos del medio */
    const float d[] = { -40.0f, -39.0f, -41.0f, 1000.0f };  /* n > 3 se recorta */

    CHECK_CLOSE(autotune_median(a, 3), 12.0f, 1e-6f);
    CHECK_CLOSE(autotune_median(b, 3), 12.0f, 1e-6f);
    CHECK_CLOSE(autotune_median(c, 2), 8.0f, 1e-6f);
    CHECK_CLOSE(autotune_median(d, 4), -40.0f, 1e-6f);   /* la fase usa tres tramas */
    CHECK_CLOSE(autotune_median(a, 0), 0.0f, 1e-6f);
    CHECK_CLOSE(autotune_median(NULL, 3), 0.0f, 1e-6f);
}

static void test_stability_spread(void) {
    const float ok[] = { 0.0f, 20.0f, 10.0f };     /* spread 20: acepta */
    const float bad[] = { 0.0f, 21.0f, 10.0f };    /* spread 21: rechaza */
    const float neg[] = { -30.0f, -50.0f, -40.0f };/* spread 20: acepta */

    CHECK(autotune_stable(ok, 3));
    CHECK(!autotune_stable(bad, 3));
    CHECK(autotune_stable(neg, 3));
    CHECK(!autotune_stable(ok, 0));
    CHECK(!autotune_stable(NULL, 3));
}

/* --- 3. delta / destino ---------------------------------------------------- */

static void test_delta_deadband_and_limits(void) {
    /* +/-(deadband): ya esta sintonizado, el core devuelve TUNED sin write. */
    CHECK(autotune_dial_delta(10.0f, AUTOTUNE_MODE_CW) == 10);
    CHECK(autotune_dial_delta(-10.0f, AUTOTUNE_MODE_CW) == -10);
    /* +/-11: requiere correccion */
    CHECK(autotune_dial_delta(11.0f, AUTOTUNE_MODE_CW) == 11);
    CHECK(autotune_dial_delta(-11.0f, AUTOTUNE_MODE_CWR) == 11);
    /* exactamente el tope pasa; el redondeo no recorta */
    CHECK(autotune_dial_delta(300.0f, AUTOTUNE_MODE_CW) == 300);
    CHECK(autotune_dial_delta(301.0f, AUTOTUNE_MODE_CW) == 301);  /* el limite lo aplica la fase */
}

static void test_delta_rounding(void) {
    CHECK(autotune_dial_delta(34.4f, AUTOTUNE_MODE_CW) == 34);
    CHECK(autotune_dial_delta(34.5f, AUTOTUNE_MODE_CW) == 35);
    CHECK(autotune_dial_delta(-34.5f, AUTOTUNE_MODE_CW) == -35);
    CHECK(autotune_dial_delta(-34.4f, AUTOTUNE_MODE_CW) == -34);
    CHECK(autotune_dial_delta(0.4f, AUTOTUNE_MODE_CW) == 0);
    CHECK(autotune_dial_delta(NAN, AUTOTUNE_MODE_CW) == 0);
    CHECK(autotune_dial_delta(INFINITY, AUTOTUNE_MODE_CW) == 0);
}

static void test_delta_sign_by_mode(void) {
    /* Contrato de la seccion 2: un tono 35 Hz por encima del pitch produce +35 Hz
     * de dial en CW y -35 Hz en CWR. */
    CHECK(autotune_dial_delta(35.0f, AUTOTUNE_MODE_CW) == 35);
    CHECK(autotune_dial_delta(35.0f, AUTOTUNE_MODE_CWR) == -35);
    CHECK(autotune_dial_delta(-35.0f, AUTOTUNE_MODE_CW) == -35);
    CHECK(autotune_dial_delta(-35.0f, AUTOTUNE_MODE_CWR) == 35);
}

static void test_target_in_band(void) {
    CHECK(autotune_target_in_band(14074000, BAND_START, BAND_STOP));
    CHECK(autotune_target_in_band(BAND_START, BAND_START, BAND_STOP));
    CHECK(autotune_target_in_band(BAND_STOP, BAND_START, BAND_STOP));
    CHECK(!autotune_target_in_band(BAND_START - 1, BAND_START, BAND_STOP));
    CHECK(!autotune_target_in_band(BAND_STOP + 1, BAND_START, BAND_STOP));
    CHECK(!autotune_target_in_band(14074000, 0, 0));          /* sin banda: rechaza */
    CHECK(!autotune_target_in_band(14074000, BAND_STOP, BAND_START));  /* rango invertido */
    CHECK(!autotune_target_in_band(-1000, BAND_START, BAND_STOP));
}

/* --- 4. sesion: tres tramas frescas y estables ----------------------------- */

static void test_phase_needs_three_frames(void) {
    autotune_t at;
    autotune_context_t ctx = ctx_of(CTX_MODE_CW, 7, 0, 700, START);

    autotune_collect_start(&at, AUTOTUNE_MODE_CW, &ctx, T0, 10);
    CHECK(at.state == AUTOTUNE_STATE_COLLECT);

    for (int i = 0; i < AUTOTUNE_PHASE_SAMPLES - 1; i++) {
        autotune_sample_t s = dsp_sample(11 + (uint64_t) i, T0 + 185 * (uint64_t) (i + 1),
                                         5.0f, 20.0f, 0.0f);
        CHECK(autotune_take(&at, &s, T0) != AUTOTUNE_TAKE_READY);
        CHECK(!autotune_phase_ready(&at));
    }
    {
        autotune_sample_t s = dsp_sample(13, T0 + 185 * 3, 5.0f, 20.0f, 0.0f);
        CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_READY);
    }
    CHECK(autotune_phase_ready(&at));
    CHECK_CLOSE(autotune_phase_median(&at), 5.0f, 1e-6f);
}

static void test_stale_duplicate_and_disordered_sequences(void) {
    autotune_t at;
    autotune_context_t ctx = ctx_of(CTX_MODE_CW, 7, 0, 700, START);
    autotune_sample_t s;

    autotune_collect_start(&at, AUTOTUNE_MODE_CW, &ctx, T0, 10);

    s = dsp_sample(11, T0 + 185, 5.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);

    /* misma secuencia (trama repetida) */
    s = dsp_sample(11, T0 + 370, 5.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_IGNORED);

    /* secuencia vieja */
    s = dsp_sample(5, T0 + 400, 5.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_IGNORED);

    /* timestamp duplicado con secuencia nueva */
    s = dsp_sample(12, T0 + 185, 5.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_IGNORED);

    /* timestamp desordenado (mas viejo que el ultimo) */
    s = dsp_sample(12, T0 + 100, 5.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_IGNORED);

    /* timestamp anterior al arranque de la fase */
    s = dsp_sample(12, T0 - 1, 5.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_IGNORED);

    /* invalidas: sin senal (relacion 0) y con overflow del estimador */
    s = dsp_sample(12, T0 + 370, 5.0f, 0.0f, 0.0f);
    s.valid = true;                        /* aunque el productor mienta */
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_IGNORED);
    s = dsp_sample(12, T0 + 370, 500.0f, 20.0f, 0.0f);
    s.valid = true;
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_IGNORED);

    /* dos tramas validas mas: la tercera (valida, fresca, estable) cierra */
    s = dsp_sample(12, T0 + 370, 5.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);
    s = dsp_sample(13, T0 + 555, 6.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_READY);
}

static void test_unstable_triple_is_not_ready(void) {
    autotune_t at;
    autotune_context_t ctx = ctx_of(CTX_MODE_CW, 7, 0, 700, START);
    uint64_t seq = 10;
    uint64_t ts = T0;

    autotune_collect_start(&at, AUTOTUNE_MODE_CW, &ctx, T0, seq);

    {
        autotune_sample_t s = dsp_sample(++seq, ts += 185, 0.0f, 20.0f, 0.0f);
        CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);
    }
    {
        autotune_sample_t s = dsp_sample(++seq, ts += 185, 25.0f, 20.0f, 0.0f);  /* spread 25 */
        CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);
    }
    {
        autotune_sample_t s = dsp_sample(++seq, ts += 185, 3.0f, 20.0f, 0.0f);
        CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);   /* 0/25/3: no estable */
        CHECK(!autotune_phase_ready(&at));
    }
    /* La ventana es corrediza: la atipica sale y tres tramas juntas cierran. */
    {
        autotune_sample_t s = dsp_sample(++seq, ts += 185, 4.0f, 20.0f, 0.0f);
        CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);   /* 25/3/4: no estable */
    }
    {
        autotune_sample_t s = dsp_sample(++seq, ts += 185, 5.0f, 20.0f, 0.0f);
        CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_READY);  /* 3/4/5: estable */
    }
}

static void test_offset_change_discards_the_phase(void) {
    autotune_t at;
    autotune_context_t ctx = ctx_of(CTX_MODE_CW, 7, 0, 700, START);
    autotune_sample_t s;

    autotune_collect_start(&at, AUTOTUNE_MODE_CW, &ctx, T0, 10);

    s = dsp_sample(11, T0 + 185, 5.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);
    s = dsp_sample(12, T0 + 370, 5.0f, 20.0f, 0.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);

    /* AUTO #139 movio el centro del DDS: las dos medidas anteriores no valen y la
     * fase se vacia (nunca se mezclan muestras de dos centros). */
    s = dsp_sample(13, T0 + 555, 5.0f, 20.0f, -30.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);
    CHECK(at.n == 1);

    s = dsp_sample(14, T0 + 740, 5.0f, 20.0f, -30.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_MORE);
    s = dsp_sample(15, T0 + 925, 5.0f, 20.0f, -30.0f);
    CHECK(autotune_take(&at, &s, T0) == AUTOTUNE_TAKE_READY);
}

/* --- 4b. signo y convergencia CW vs CWR (extremo a extremo del nucleo) ----- */

static void test_cw_tone_35_above_pitch_converges(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);

    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    /* Un unico write: +35 Hz en CW. */
    CHECK(r.writes == 1);
    CHECK(r.last_write == START + 35);
    CHECK(r.dial == START + 35);
    CHECK(r.at.state == AUTOTUNE_STATE_VERIFY);

    /* La fisica del receptor deja el tono ~0 Hz del pitch. */
    CHECK_CLOSE(r.tone_error, 0.0f, 1e-6f);

    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    CHECK(r.outcome == RIG_TUNED);
    CHECK(r.writes == 1);                  /* nunca una segunda correccion */
    CHECK(r.dial == START + 35);
    CHECK_CLOSE(r.tone_error, 0.0f, 1e-6f);
}

static void test_cwr_tone_35_above_pitch_converges(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CWR, START, 35.0f);

    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    /* Mismo tono, sentido INVERSO del dial. */
    CHECK(r.writes == 1);
    CHECK(r.last_write == START - 35);
    CHECK_CLOSE(r.tone_error, 0.0f, 1e-6f);

    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    CHECK(r.outcome == RIG_TUNED);
    CHECK(r.writes == 1);
    CHECK(r.dial == START - 35);
}

static void test_cw_and_cwr_tone_35_below_pitch(void) {
    rig_t r;

    /* Tono 35 Hz por DEBAJO del pitch: CW baja el dial, CWR lo sube. */
    rig_start(&r, AUTOTUNE_MODE_CW, START, -35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    CHECK(r.last_write == START - 35);

    rig_start(&r, AUTOTUNE_MODE_CWR, START, -35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    CHECK(r.last_write == START + 35);
}

static void test_offset_does_not_change_the_target(void) {
    rig_t a;
    rig_t b;

    /* Con el AUTO del decodificador moviendo el DDS (rx_tone_offset != 0), el
     * destino sigue saliendo del PITCH manual: el error llega ya compensado desde
     * la capa DSP, asi que el offset no cambia ni el signo ni el destino. */
    rig_start(&a, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&a, a.tone_error, 20.0f);
    rig_normal_frame(&a, a.tone_error, 20.0f);
    rig_normal_frame(&a, a.tone_error, 20.0f);

    rig_start(&b, AUTOTUNE_MODE_CW, START, 35.0f);
    b.dds_offset = -30.0f;
    rig_normal_frame(&b, b.tone_error, 20.0f);
    rig_normal_frame(&b, b.tone_error, 20.0f);
    rig_normal_frame(&b, b.tone_error, 20.0f);

    CHECK(a.last_write == b.last_write);
    CHECK(a.last_write == START + 35);
}

/* --- 5. decisiones: deadband, tope, banda --------------------------------- */

static void test_deadband_no_write(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 10.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    CHECK(r.outcome == RIG_TUNED);
    CHECK(r.writes == 0);
    CHECK(r.dial == START);
}

static void test_over_300_does_not_move_the_dial(void) {
    rig_t r;

    /* 320 Hz de desvio (dentro del rango del estimador, por encima del tope): NO
     * se recorta a 300, no se toca el dial. */
    rig_start(&r, AUTOTUNE_MODE_CW, START, 320.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    CHECK(r.outcome == RIG_FAIL);
    CHECK(r.writes == 0);
    CHECK(r.dial == START);

    /* Desvio fuera del rango del estimador: la trama ni entra (el DSP la marca
     * invalida) y la fase vence sin escribir. */
    rig_start(&r, AUTOTUNE_MODE_CW, START, 500.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_idle(&r, AUTOTUNE_PHASE_TIMEOUT_MS + 1);

    CHECK(r.writes == 0);
    CHECK(r.dial == START);
}

static void test_exactly_300_moves(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 300.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    CHECK(r.writes == 1);
    CHECK(r.last_write == START + 300);
}

static void test_band_reject_no_write(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    /* El destino (+35 Hz) cae fuera del rango de la banda configurada. */
    r.band_start = START - 10;
    r.band_stop = START + 10;

    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    CHECK(r.outcome == RIG_FAIL);
    CHECK(r.writes == 0);
    CHECK(r.dial == START);
}

/* --- 6. VERIFY: guarda de 300 ms, tramas nuevas, rollback ------------------ */

static void test_verify_needs_guard_and_new_frames(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    CHECK(r.at.state == AUTOTUNE_STATE_VERIFY);

    /* Trama publicada en el mismo instante de la escritura y otra a 185 ms: las
     * dos caen dentro de la guarda de 300 ms. No entran (la secuencia es nueva). */
    {
        autotune_sample_t s = dsp_sample(r.seq++, r.now, r.tone_error, 20.0f, r.dds_offset);
        CHECK(autotune_take(&r.at, &s, r.now) == AUTOTUNE_TAKE_IGNORED);
        s = dsp_sample(r.seq++, r.now + 185, r.tone_error, 20.0f, r.dds_offset);
        CHECK(autotune_take(&r.at, &s, r.now) == AUTOTUNE_TAKE_IGNORED);
        CHECK(r.at.n == 0);
    }

    /* A los 400 ms de la escritura si entra; hacen falta tres NUEVAS. */
    rig_idle(&r, 215);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    CHECK(r.at.state == AUTOTUNE_STATE_VERIFY);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    CHECK(r.outcome == RIG_TUNED);
    CHECK(r.writes == 1);
}

static void test_verify_rolls_back_when_it_does_not_converge(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    CHECK(r.dial == START + 35);

    /* La correccion no verifico (el tono no se movio con el dial): rollback. */
    rig_normal_frame(&r, 250.0f, 20.0f);
    rig_normal_frame(&r, 250.0f, 20.0f);
    rig_normal_frame(&r, 250.0f, 20.0f);

    CHECK(r.outcome == RIG_FAIL);
    CHECK(r.writes == 2);                   /* una provisional + un rollback */
    CHECK(r.dial == START);
    CHECK(r.at.state == AUTOTUNE_STATE_IDLE);
}

static void test_session_and_phase_timeouts(void) {
    autotune_t at;
    autotune_context_t ctx = ctx_of(CTX_MODE_CW, 7, 0, 700, START);

    /* Fase: 3000 ms desde el arranque de la fase. */
    autotune_collect_start(&at, AUTOTUNE_MODE_CW, &ctx, T0, 10);
    CHECK(!autotune_phase_timeout(&at, T0 + AUTOTUNE_PHASE_TIMEOUT_MS));
    CHECK(autotune_phase_timeout(&at, T0 + AUTOTUNE_PHASE_TIMEOUT_MS + 1));
    CHECK(!autotune_session_timeout(&at, T0 + AUTOTUNE_SESSION_TIMEOUT_MS));
    CHECK(autotune_session_timeout(&at, T0 + AUTOTUNE_SESSION_TIMEOUT_MS + 1));
}

static void test_timeout_ends_without_write(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    /* Sin ninguna trama: al pasar el tope de la fase termina sin escribir. */
    rig_idle(&r, AUTOTUNE_PHASE_TIMEOUT_MS + 1);
    CHECK(r.outcome == RIG_FAIL);
    CHECK(r.writes == 0);
    CHECK(r.dial == START);
    CHECK(r.at.state == AUTOTUNE_STATE_IDLE);
}

static void test_timeout_during_verify_rolls_back(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    CHECK(r.dial == START + 35);

    rig_idle(&r, AUTOTUNE_SESSION_TIMEOUT_MS + 1);
    CHECK(r.outcome == RIG_FAIL);
    CHECK(r.dial == START);                 /* rollback condicionado */
}

/* --- 7. propiedad del dial: cancelaciones y cambios externos --------------- */

static void test_two_triggers_and_cancel_paths(void) {
    rig_t r;

    /* Cancelar en COLLECT (sin write): nada que restaurar. */
    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    CHECK(r.at.state == AUTOTUNE_STATE_COLLECT);
    rig_end(&r, true);
    CHECK(r.dial == START);
    CHECK(r.writes == 0);

    /* Cancelar despues de la provisional: rollback al arranque. */
    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    CHECK(r.dial == START + 35);
    rig_end(&r, true);
    CHECK(r.dial == START);
    CHECK(r.writes == 2);
    CHECK(r.at.state == AUTOTUNE_STATE_IDLE);
}

static void test_manual_dial_wins_during_collect(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&r, 35.0f, 20.0f);

    /* El operador gira el dial: el coordinador aborta ANTES de escribir y sin
     * restaurar. El estado nuevo es del operador. */
    r.dial = START + 2000;
    r.outcome = RIG_CANCELLED;
    rig_end(&r, false);

    CHECK(r.dial == START + 2000);
    CHECK(r.writes == 0);
}

static void test_manual_dial_wins_during_verify(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    CHECK(r.dial == START + 35);
    CHECK(r.at.state == AUTOTUNE_STATE_VERIFY);

    /* Giro manual durante VERIFY: aborta sin rollback (jamas se pisa el QSY). */
    r.dial = START - 5000;
    r.outcome = RIG_CANCELLED;
    rig_end(&r, false);

    CHECK(r.dial == START - 5000);
    CHECK(r.writes == 1);
}

static void test_external_freq_during_verify_aborts_without_rollback(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    /* Cambio externo (CAT / memoria / app) a otra frecuencia: abortar sin
     * escribir nada. */
    r.dial = START + 12000;
    rig_sync(&r);
    CHECK(!autotune_owns(&r.at, &r.ctx));
    r.outcome = RIG_CANCELLED;
    rig_end(&r, true);                      /* aunque se pida restore, no puede */
    CHECK(r.dial == START + 12000);
}

static void test_context_changes_break_the_session(void) {
    autotune_t at;
    autotune_context_t ctx = ctx_of(CTX_MODE_CW, 7, 0, 700, START);
    autotune_context_t now;

    autotune_collect_start(&at, AUTOTUNE_MODE_CW, &ctx, T0, 10);
    CHECK(!autotune_context_broken(&at, &ctx));
    CHECK(autotune_owns(&at, &ctx));

    now = ctx; now.mode = 3;                 /* modo distinto */
    CHECK(autotune_context_broken(&at, &now));
    CHECK(!autotune_owns(&at, &now));

    now = ctx; now.band_id = 8;              /* banda distinta */
    CHECK(autotune_context_broken(&at, &now));

    now = ctx; now.vfo = 1;                  /* VFO A/B */
    CHECK(autotune_context_broken(&at, &now));

    now = ctx; now.pitch = 800;              /* pitch manual */
    CHECK(autotune_context_broken(&at, &now));

    now = ctx; now.rx = false;               /* TX / ATU / SWR / power off */
    CHECK(autotune_context_broken(&at, &now));

    now = ctx; now.locked = true;            /* lock de frecuencia */
    CHECK(autotune_context_broken(&at, &now));

    now = ctx; now.rit = true;               /* RIT distinto de cero */
    CHECK(autotune_context_broken(&at, &now));

    now = ctx; now.freq = START + 100;       /* solo el dial: NO rompe el contexto */
    CHECK(!autotune_context_broken(&at, &now));
    CHECK(!autotune_owns(&at, &now));        /* pero ya no es dueño */

    autotune_reset(&at);
    CHECK(autotune_context_broken(&at, &ctx));   /* sin sesion no hay contexto */
}

static void test_rit_during_session_cancels_without_write(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    r.ctx.rit = true;
    rig_idle(&r, 50);

    CHECK(r.outcome == RIG_CANCELLED);
    CHECK(r.writes == 0);
    CHECK(r.dial == START);
}

static void test_tx_during_verify_never_writes(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    int32_t after_write = r.dial;

    /* TX activo: aborta SIN sintonizar (ni el rollback). */
    r.ctx.rx = false;
    r.outcome = RIG_CANCELLED;
    rig_end(&r, false);

    CHECK(r.dial == after_write);
    CHECK(r.writes == 1);
}

/* --- 8. frescura: nunca se reutiliza una trama vieja ---------------------- */

static void test_no_stale_frame_reuse_after_retune(void) {
    rig_t r;

    rig_start(&r, AUTOTUNE_MODE_CW, START, 35.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);
    rig_normal_frame(&r, r.tone_error, 20.0f);

    uint64_t last_seq = r.at.last_seq;
    uint64_t min_ms = r.at.min_ms;

    /* Un frame viejo (secuencia <= la ultima usada y timestamp anterior a la
     * escritura) se ignora aunque llegue despues. */
    {
        autotune_sample_t s = dsp_sample(last_seq - 1, min_ms - 1, 5.0f, 20.0f, r.dds_offset);
        CHECK(autotune_take(&r.at, &s, r.now) == AUTOTUNE_TAKE_IGNORED);
        s = dsp_sample(last_seq, min_ms - 1, 5.0f, 20.0f, r.dds_offset);
        CHECK(autotune_take(&r.at, &s, r.now) == AUTOTUNE_TAKE_IGNORED);
    }
    /* Y una trama fresca despues de la guarda si entra. */
    {
        autotune_sample_t s = dsp_sample(last_seq + 1, min_ms + 1, 5.0f, 20.0f, r.dds_offset);
        CHECK(autotune_take(&r.at, &s, r.now) == AUTOTUNE_TAKE_MORE);
    }
    CHECK(r.at.last_seq == last_seq + 1);
}

void test_autotune(void) {
    test_sample_gate();
    test_median_robust_to_outlier();
    test_stability_spread();
    test_delta_deadband_and_limits();
    test_delta_rounding();
    test_delta_sign_by_mode();
    test_target_in_band();
    test_phase_needs_three_frames();
    test_stale_duplicate_and_disordered_sequences();
    test_unstable_triple_is_not_ready();
    test_offset_change_discards_the_phase();
    test_cw_tone_35_above_pitch_converges();
    test_cwr_tone_35_above_pitch_converges();
    test_cw_and_cwr_tone_35_below_pitch();
    test_offset_does_not_change_the_target();
    test_deadband_no_write();
    test_over_300_does_not_move_the_dial();
    test_exactly_300_moves();
    test_band_reject_no_write();
    test_verify_needs_guard_and_new_frames();
    test_verify_rolls_back_when_it_does_not_converge();
    test_session_and_phase_timeouts();
    test_timeout_ends_without_write();
    test_timeout_during_verify_rolls_back();
    test_two_triggers_and_cancel_paths();
    test_manual_dial_wins_during_collect();
    test_manual_dial_wins_during_verify();
    test_external_freq_during_verify_aborts_without_rollback();
    test_context_changes_break_the_session();
    test_rit_during_session_cancels_without_write();
    test_tx_during_verify_never_writes();
    test_no_stale_frame_reuse_after_retune();
}

/*
 * Unit tests: the multimode spectrum policies of Auto Tune (issue #205) - the
 * AM/SAM carrier estimator, the USB/LSB energy suggestion, the freshness/
 * context gate and the phase windows (COLLECT/VERIFY/PREVIEW/UNDO). Pure C, no
 * LVGL, no radio, no DSP: synthetic spectra with exact dB values.
 *
 * The CW/CWR core keeps its own suite (tests/native/test_autotune.c, 169 checks)
 * and is not touched here: this file only exercises the new pure module.
 *
 * Numeric contract (issue #205 sections 4 to 6):
 *   - N=800 bins, center 400, offset_hz(i) = (i - 400) * width_hz / 800.
 *   - The frame INTERPOLATES 512 physical bins: resolution = width_hz / 512 and
 *     the candidates are quantized to step = max(1, round(resolution)). The
 *     100 kHz view therefore has step 195 Hz (NOT 125 Hz = 100000/800).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <string.h>

#include "autotune.h"
#include "autotune_spectrum.h"
#include "test_framework.h"

#define START      14074000
#define W_100K     100000      /* resolucion 195.3125 Hz -> step 195 */
#define W_96K      96000       /* resolucion 187.5 Hz    -> step 188 */
#define W_FULL     384000      /* resolucion 750 Hz: no medible */
/* Los modos son valores persistentes de x6200_mode_t (aether-x6200-control). El
 * modulo puro los trata como enteros opacos: solo los compara y los publica. */
#define MODE_LSB   0
#define MODE_USB   2
#define MODE_AM    6
#define MODE_SAM   7
#define NOISE_DB   -100.0f
#define PEAK_DB    -60.0f
#define VOICE_DB   -75.0f

static dsp_autotune_spectrum_t s;

/* --- construccion de espectros sinteticos ---------------------------------- */

static dsp_autotune_spectrum_t *snap(int32_t width, int32_t mode) {
    memset(&s, 0, sizeof(s));
    s.sequence = 1;
    s.timestamp_ms = 1000;
    s.epoch = 3;
    s.center_freq_hz = START;
    s.mode = mode;
    s.width_hz = width;
    s.valid = true;
    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        s.db[i] = NOISE_DB;
    }
    return &s;
}

/* Bin de display mas cercano al offset pedido. */
static int32_t bin_of(float offset_hz) {
    return AUTOTUNE_SPECTRUM_CENTER + (int32_t) lroundf(offset_hz / ((float) s.width_hz / 800.0f));
}

static void carrier(float offset_hz, float level_db) {
    s.db[bin_of(offset_hz)] = level_db;
}

/* Masa de energia (la voz de USB/LSB) centrada en `center_hz`. */
static void energy(float center_hz, float width_hz, float level_db) {
    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        float off = autotune_spectrum_offset_hz(i, s.width_hz);

        if (fabsf(off - center_hz) <= width_hz / 2.0f) {
            s.db[i] = level_db;
        }
    }
}

static int32_t am_candidate(void) {
    int32_t off = 12345;

    CHECK(autotune_spectrum_am_candidate(&s, &off));
    return off;
}

static bool ssb_candidate(int32_t from, int32_t to, bool lsb, int32_t *delta) {
    return autotune_spectrum_ssb_candidate(&s, from, to, lsb, delta);
}

/* --- 1. geometria: la resolucion es la fisica (512), no la visual (800) ---- */

static void test_coordinates_and_resolution(void) {
    CHECK_CLOSE(autotune_spectrum_offset_hz(400, W_100K), 0.0f, 1e-3f);
    CHECK_CLOSE(autotune_spectrum_offset_hz(408, W_100K), 1000.0f, 1e-3f);
    CHECK_CLOSE(autotune_spectrum_offset_hz(0, W_100K), -50000.0f, 1e-3f);

    /* 100000/512 = 195.3125 (fisica) y 100000/800 = 125 (visual): el contrato
     * prohibe inferir precision de los 800. */
    CHECK_CLOSE(autotune_spectrum_resolution_hz(W_100K), 195.3125f, 1e-3f);
    CHECK_CLOSE(autotune_spectrum_resolution_hz(W_96K), 187.5f, 1e-3f);
    CHECK(autotune_spectrum_step_hz(W_100K) == 195);
    CHECK(autotune_spectrum_step_hz(W_96K) == 188);
    CHECK(autotune_spectrum_step_hz(W_100K) != 125);
    CHECK(autotune_spectrum_step_hz(1000) >= 1);
}

static void test_measurable_limits(void) {
    snap(W_100K, MODE_AM);
    CHECK(autotune_spectrum_measurable(&s));
    snap(W_96K, MODE_AM);
    CHECK(autotune_spectrum_measurable(&s));

    /* Por encima de 100 kHz no se mide (seccion 5). La resolucion de 250 Hz es
     * el tope redundante: con el ancho limitado a 100 kHz la resolucion maxima
     * es 195.3 Hz, asi que ese guardia no puede aflojarse nunca. */
    CHECK(autotune_spectrum_resolution_hz(100000) <= AUTOTUNE_SPECTRUM_MAX_RESOLUTION_HZ);
    CHECK(autotune_spectrum_resolution_hz(150000) > AUTOTUNE_SPECTRUM_MAX_RESOLUTION_HZ);
    snap(128000, MODE_AM);
    CHECK(!autotune_spectrum_measurable(&s));
    snap(130000, MODE_AM);
    CHECK(!autotune_spectrum_measurable(&s));
    snap(W_FULL, MODE_AM);
    CHECK(!autotune_spectrum_measurable(&s));
    snap(0, MODE_AM);
    CHECK(!autotune_spectrum_measurable(&s));
    snap(-1000, MODE_AM);
    CHECK(!autotune_spectrum_measurable(&s));

    /* TX y una publicacion marcada invalida no se miden nunca. */
    snap(W_100K, MODE_AM);
    s.tx = true;
    CHECK(!autotune_spectrum_measurable(&s));
    snap(W_100K, MODE_AM);
    s.valid = false;
    CHECK(!autotune_spectrum_measurable(&s));

    CHECK(!autotune_spectrum_measurable(NULL));
}

/* --- 2. AM/SAM: portadora estrecha, unica y dominante --------------------- */

static void test_am_flat_and_silence(void) {
    int32_t off = 0;

    snap(W_100K, MODE_AM);
    CHECK(!autotune_spectrum_am_candidate(&s, NULL));
    CHECK(!autotune_spectrum_am_candidate(&s, &off));       /* PSD plano: sin pico */
    snap(W_100K, MODE_AM);
    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        s.db[i] = -200.0f;                                  /* silencio */
    }
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
}

static void test_am_single_carrier(void) {
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    CHECK(am_candidate() == 390);       /* 375 Hz al bin mas cercano, cuantizado a 195 */

    snap(W_100K, MODE_AM);
    carrier(750.0f, PEAK_DB);
    CHECK(am_candidate() == 780);

    snap(W_100K, MODE_AM);
    carrier(-375.0f, PEAK_DB);
    CHECK(am_candidate() == -390);

    snap(W_100K, MODE_AM);
    carrier(-750.0f, PEAK_DB);
    CHECK(am_candidate() == -780);

    /* La vista de 96 kHz de la radio (step 188). */
    snap(W_96K, MODE_AM);
    carrier(376.0f, PEAK_DB);
    CHECK(am_candidate() == 376);
    snap(W_96K, MODE_AM);
    carrier(752.0f, PEAK_DB);
    CHECK(am_candidate() == 752);
}

static void test_am_already_centered_and_at_the_edge(void) {
    snap(W_100K, MODE_AM);
    carrier(0.0f, PEAK_DB);
    CHECK(am_candidate() == 0);

    /* El borde de la ventana de portadora (+/-1000 Hz): el candidato cuantizado
     * cae dentro del tope de la correccion. */
    snap(W_100K, MODE_AM);
    carrier(1000.0f, PEAK_DB);
    CHECK(am_candidate() == 975);
    snap(W_100K, MODE_AM);
    carrier(-1000.0f, PEAK_DB);
    CHECK(am_candidate() == -975);
}

static void test_am_outside_the_window_is_not_a_carrier(void) {
    /* Una senal fuerte FUERA de +/-1000 Hz no es una portadora de AM: la ventana
     * queda plana y un PSD plano se rechaza. */
    snap(W_100K, MODE_AM);
    carrier(2500.0f, -40.0f);
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
    snap(W_100K, MODE_AM);
    carrier(-3000.0f, -40.0f);
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
}

static void test_am_thresholds(void) {
    /* peak - noise: 11 dB NO alcanza, 12 dB si (seccion 5.2). */
    snap(W_100K, MODE_AM);
    carrier(375.0f, NOISE_DB + 11.0f);
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
    snap(W_100K, MODE_AM);
    carrier(375.0f, NOISE_DB + 12.0f);
    CHECK(am_candidate() == 390);

    /* Dominancia: un segundo pico a 3 dB (y a mas de 2*resolucion) rechaza. */
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    carrier(875.0f, PEAK_DB - 3.0f);
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
    /* Empate exacto de dos portadoras: tampoco. */
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    carrier(875.0f, PEAK_DB);
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
    /* Con 6 dB exactos de diferencia ya hay una portadora dominante. */
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    carrier(875.0f, PEAK_DB - 6.0f);
    CHECK(am_candidate() == 390);

    /* Picos SUPERPUESTOS (separados menos que la resolucion fisica): son una
     * sola portadora, no un empate. */
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    carrier(500.0f, PEAK_DB);
    CHECK(am_candidate() == 390);
}

static void test_am_non_finite_rejects(void) {
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    s.db[bin_of(375.0f)] = NAN;
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    s.db[bin_of(4000.0f)] = INFINITY;               /* piso de ruido */
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
    snap(W_100K, MODE_AM);
    carrier(375.0f, INFINITY);                      /* pico no finito */
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
}

static void test_am_too_wide_is_not_measurable(void) {
    snap(W_FULL, MODE_AM);
    carrier(375.0f, PEAK_DB);
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    s.valid = false;                                /* publicacion no medible */
    {
        int32_t off = 0;

        CHECK(!autotune_spectrum_am_candidate(&s, &off));
    }
}

/* --- 3. AM/SAM: coleccion de cuatro tramas y veredicto --------------------- */

static autotune_am_state_t am_state(void) {
    autotune_am_state_t st;

    autotune_am_reset(&st);
    return st;
}

static void test_am_collect_four_frames(void) {
    autotune_am_state_t st = am_state();
    int32_t             delta = 0;

    for (int i = 0; i < AUTOTUNE_AM_FRAMES; i++) {
        snap(W_100K, MODE_AM);
        carrier(375.0f, PEAK_DB);
        if (i < AUTOTUNE_AM_FRAMES - 1) {
            CHECK(autotune_am_take(&st, &s) == AUTOTUNE_STEP_COLLECTING);
        } else {
            CHECK(autotune_am_take(&st, &s) == AUTOTUNE_STEP_READY);
        }
    }
    CHECK(autotune_am_verdict(&st, &delta) == AUTOTUNE_VERDICT_MOVE);
    CHECK(delta == 390);

    /* Ya centrado: CENTERED y cero escrituras. */
    st = am_state();
    for (int i = 0; i < AUTOTUNE_AM_FRAMES; i++) {
        snap(W_100K, MODE_AM);
        carrier(0.0f, PEAK_DB);
        CHECK(autotune_am_take(&st, &s) == (i < AUTOTUNE_AM_FRAMES - 1
                                               ? AUTOTUNE_STEP_COLLECTING
                                               : AUTOTUNE_STEP_READY));
    }
    CHECK(autotune_am_verdict(&st, &delta) == AUTOTUNE_VERDICT_CENTERED);
}

static void test_am_collect_inconsistent_frames(void) {
    autotune_am_state_t st = am_state();
    int32_t             delta = 0;

    /* Dos portadoras alternadas (spread 390 > resolucion 195.3): la ventana
     * corrediza nunca junta cuatro consistentes, asi que la fase sigue
     * COLLECTING y el coordinador no consulta el veredicto (que venceria con
     * NONE). Que el estado que queda tenga mediana 390 no importa: sin READY no
     * hay veredicto. */
    for (int i = 0; i < 8; i++) {
        snap(W_100K, MODE_AM);
        carrier((i % 2) ? 750.0f : 375.0f, PEAK_DB);
        CHECK(autotune_am_take(&st, &s) != AUTOTUNE_STEP_READY);
    }
    CHECK(st.n == AUTOTUNE_AM_FRAMES);
    CHECK(autotune_am_take(&st, &s) == AUTOTUNE_STEP_COLLECTING);
    (void) delta;

    /* Un candidato de signo opuesto no entra (no se mezclan lados). */
    st = am_state();
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    CHECK(autotune_am_take(&st, &s) == AUTOTUNE_STEP_COLLECTING);
    snap(W_100K, MODE_AM);
    carrier(-375.0f, PEAK_DB);
    CHECK(autotune_am_take(&st, &s) == AUTOTUNE_STEP_REJECTED);
}

static void test_am_frames_rejected_by_the_policy(void) {
    autotune_am_state_t st = am_state();

    snap(W_100K, MODE_AM);                    /* PSD plano */
    CHECK(autotune_am_take(&st, &s) == AUTOTUNE_STEP_REJECTED);
    CHECK(autotune_am_take(&st, NULL) == AUTOTUNE_STEP_REJECTED);
    CHECK(autotune_am_take(NULL, &s) == AUTOTUNE_STEP_REJECTED);
}

static void test_am_verdict_bounds(void) {
    autotune_am_state_t st = am_state();
    int32_t             delta = 0;

    /* Por encima del tope de la unica correccion: NONE (no se recorta). */
    st.n = AUTOTUNE_AM_FRAMES;
    st.have_resolution = true;
    st.resolution_hz = 195.3125f;
    for (int i = 0; i < AUTOTUNE_AM_FRAMES; i++) {
        st.offset_hz[i] = 1170;
    }
    CHECK(autotune_am_verdict(&st, &delta) == AUTOTUNE_VERDICT_NONE);

    /* Y el tope exacto pasa. */
    for (int i = 0; i < AUTOTUNE_AM_FRAMES; i++) {
        st.offset_hz[i] = AUTOTUNE_AM_MAX_DELTA_HZ;
    }
    CHECK(autotune_am_verdict(&st, &delta) == AUTOTUNE_VERDICT_MOVE);
    CHECK(delta == AUTOTUNE_AM_MAX_DELTA_HZ);

    /* Sin cuatro tramas no hay veredicto. */
    st.n = 3;
    CHECK(autotune_am_verdict(&st, &delta) == AUTOTUNE_VERDICT_NONE);
}

static void test_am_verified_needs_fresh_centered_frames(void) {
    autotune_am_state_t st = am_state();

    /* Cuatro tramas nuevas ya centradas: verificado. */
    for (int i = 0; i < AUTOTUNE_AM_FRAMES; i++) {
        st.offset_hz[i] = 0;
    }
    st.n = AUTOTUNE_AM_FRAMES;
    st.resolution_hz = 195.3125f;
    CHECK(autotune_spectrum_am_verified(&st));

    /* La correccion no movio la portadora (quedo a 390 Hz): no verifica. */
    for (int i = 0; i < AUTOTUNE_AM_FRAMES; i++) {
        st.offset_hz[i] = 390;
    }
    CHECK(!autotune_spectrum_am_verified(&st));

    /* Dentro de la resolucion (94 Hz por debajo de 97.6): verifica. */
    for (int i = 0; i < AUTOTUNE_AM_FRAMES; i++) {
        st.offset_hz[i] = 94;
    }
    CHECK(autotune_spectrum_am_verified(&st));

    st.n = AUTOTUNE_AM_FRAMES - 1;
    CHECK(!autotune_spectrum_am_verified(&st));
    CHECK(!autotune_spectrum_am_verified(NULL));
}

/* --- 4. gate de frescura y contexto --------------------------------------- */

static dsp_autotune_spectrum_t frame(uint64_t seq, uint64_t ts, uint64_t epoch, int32_t center) {
    dsp_autotune_spectrum_t f;

    memset(&f, 0, sizeof(f));
    f.sequence = seq;
    f.timestamp_ms = ts;
    f.epoch = epoch;
    f.center_freq_hz = center;
    f.mode = MODE_AM;
    f.width_hz = W_100K;
    f.valid = true;
    return f;
}

static void test_gate_freshness(void) {
    autotune_frame_gate_t g;
    dsp_autotune_spectrum_t f;

    autotune_frame_gate_reset(&g);
    autotune_frame_gate_arm(&g, 1000, 0, 0);

    CHECK(autotune_frame_gate_take(&g, NULL) == AUTOTUNE_FRAME_SKIP);
    f = frame(10, 1000, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_TAKEN);
    /* Repetida, desordenada o con timestamp igual: afuera. */
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_SKIP);
    f.sequence = 11;
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_SKIP);
    f = frame(11, 1100, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_TAKEN);
    f = frame(12, 1090, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_SKIP);

    /* Piso de tiempo (guarda post-escritura) y descarte de las N posteriores. */
    autotune_frame_gate_arm(&g, 2000, 20, 0);
    f = frame(21, 1999, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_SKIP);
    f = frame(20, 2001, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_SKIP);
    f = frame(21, 2001, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_TAKEN);

    /* Epoch exigido: una publicacion anterior a la escritura propia no entra. */
    autotune_frame_gate_arm(&g, 0, 0, 7);
    f = frame(30, 3000, 6, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_SKIP);
    f = frame(31, 3010, 7, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_TAKEN);
}

static void test_gate_context_and_tx(void) {
    autotune_frame_gate_t g;
    dsp_autotune_spectrum_t f;

    autotune_frame_gate_reset(&g);
    autotune_frame_gate_arm(&g, 0, 0, 0);

    f = frame(1, 100, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_TAKEN);

    /* Otro epoch: la medida no es comparable. */
    f = frame(2, 200, 6, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_BROKEN);
    /* Otro centro (QSY), otro modo o otro ancho: tampoco. */
    autotune_frame_gate_reset(&g);
    autotune_frame_gate_arm(&g, 0, 0, 0);
    f = frame(1, 100, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_TAKEN);
    f = frame(2, 200, 5, START + 1000);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_BROKEN);
    autotune_frame_gate_reset(&g);
    autotune_frame_gate_arm(&g, 0, 0, 0);
    f = frame(1, 100, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_TAKEN);
    f = frame(2, 200, 5, START);
    f.mode = MODE_SAM;
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_BROKEN);
    autotune_frame_gate_reset(&g);
    autotune_frame_gate_arm(&g, 0, 0, 0);
    f = frame(1, 100, 5, START);
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_TAKEN);
    f = frame(2, 200, 5, START);
    f.width_hz = W_96K;
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_BROKEN);

    /* TX y publicacion invalida: nunca entran. */
    autotune_frame_gate_reset(&g);
    autotune_frame_gate_arm(&g, 0, 0, 0);
    f = frame(1, 100, 5, START);
    f.tx = true;
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_SKIP);
    f.tx = false;
    f.valid = false;
    CHECK(autotune_frame_gate_take(&g, &f) == AUTOTUNE_FRAME_SKIP);
}
/* --- 5. USB/LSB: sugerencia gruesa con confirmacion ----------------------- */

static void test_ssb_filter_contract(void) {
    CHECK(autotune_spectrum_ssb_filter_ok(0, 3000, false));      /* USB */
    CHECK(autotune_spectrum_ssb_filter_ok(-3000, 0, true));      /* LSB */
    CHECK(!autotune_spectrum_ssb_filter_ok(-3000, 0, false));    /* lado cambiado */
    CHECK(!autotune_spectrum_ssb_filter_ok(0, 3000, true));
    CHECK(!autotune_spectrum_ssb_filter_ok(0, 1500, false));     /* angosto */
    CHECK(!autotune_spectrum_ssb_filter_ok(0, 3600, false));     /* ancho */
    CHECK(!autotune_spectrum_ssb_filter_ok(-3000, 0, false));
    CHECK(!autotune_spectrum_ssb_filter_ok(0, 0, false));
}

static void test_ssb_usb_voice_outside_the_passband(void) {
    int32_t delta = 0;

    /* Voz ocupando 2100..3600 Hz (fuera del pasante 300..2700): el mejor
     * desplazamiento es positivo y del orden de los 750 Hz. */
    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    CHECK(ssb_candidate(0, 3000, false, &delta));
    CHECK(delta == 752);
    CHECK(delta >= AUTOTUNE_SSB_MIN_DELTA_HZ);

    snap(W_100K, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    CHECK(ssb_candidate(0, 3000, false, &delta));
    CHECK(delta == 780);
}

static void test_ssb_lsb_is_mirrored(void) {
    int32_t delta = 0;

    /* Misma voz, del otro lado del dial: el desplazamiento cambia de signo. */
    snap(W_96K, MODE_LSB);
    energy(-2850.0f, 1500.0f, VOICE_DB);
    CHECK(ssb_candidate(-3000, 0, true, &delta));
    CHECK(delta == -752);

    /* Y con el filtro real de la banda lateral opuesta, no hay sugerencia. */
    snap(W_96K, MODE_LSB);
    energy(-2850.0f, 1500.0f, VOICE_DB);
    CHECK(!ssb_candidate(0, 3000, true, &delta));
}

static void test_ssb_abstains(void) {
    int32_t delta = 0;

    /* Energia YA dentro del pasante: no hay nada que alinear (nunca se sugiere
     * cero). */
    snap(W_96K, MODE_USB);
    energy(1300.0f, 900.0f, VOICE_DB);
    CHECK(!ssb_candidate(0, 3000, false, &delta));

    /* Energia lejisimos de todo candidato. */
    snap(W_96K, MODE_USB);
    energy(5000.0f, 600.0f, VOICE_DB);
    CHECK(!ssb_candidate(0, 3000, false, &delta));

    /* DOS estaciones con energia parecida: sin propuesta unica. */
    snap(W_96K, MODE_USB);
    energy(1600.0f, 1200.0f, VOICE_DB);
    energy(3300.0f, 1200.0f, VOICE_DB);
    CHECK(!ssb_candidate(0, 3000, false, &delta));

    /* Un bloque ancho (voz no estacionaria dentro de la ventana): tampoco. */
    snap(W_96K, MODE_USB);
    energy(1600.0f, 2400.0f, VOICE_DB);
    CHECK(!ssb_candidate(0, 3000, false, &delta));
}

static void test_ssb_score_threshold(void) {
    int32_t delta = 0;

    /* El score minimo es 5.0: con la misma voz, 12 dB sobre el piso no alcanza y
     * 13 dB si. Es un umbral de ENERGIA, no de calidad de voz. */
    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, NOISE_DB + 12.0f);
    CHECK(!ssb_candidate(0, 3000, false, &delta));
    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, NOISE_DB + 13.0f);
    CHECK(ssb_candidate(0, 3000, false, &delta));
    CHECK(delta == 752);
}

static void test_ssb_measurement_conditions(void) {
    int32_t delta = 0;

    snap(W_FULL, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    CHECK(!ssb_candidate(0, 3000, false, &delta));

    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    s.valid = false;
    CHECK(!ssb_candidate(0, 3000, false, &delta));
    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    s.tx = true;
    CHECK(!ssb_candidate(0, 3000, false, &delta));

    /* Filtros fuera del contrato: no se opina. */
    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    CHECK(!ssb_candidate(0, 1500, false, &delta));
    CHECK(!ssb_candidate(-3000, 0, false, &delta));
    CHECK(!ssb_candidate(0, 3600, false, &delta));

    /* No finitos en la zona que se puntua. */
    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    s.db[AUTOTUNE_SPECTRUM_CENTER] = NAN;
    CHECK(!ssb_candidate(0, 3000, false, &delta));
    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    s.db[bin_of(4000.0f)] = INFINITY;
    CHECK(!ssb_candidate(0, 3000, false, &delta));
    CHECK(!ssb_candidate(0, 3000, false, NULL));
}

static void test_ssb_collect_five_frames(void) {
    autotune_ssb_state_t st;
    int32_t              delta = 0;

    autotune_ssb_reset(&st);
    for (int i = 0; i < AUTOTUNE_SSB_FRAMES; i++) {
        snap(W_96K, MODE_USB);
        energy(2850.0f, 1500.0f, VOICE_DB);
        if (i < AUTOTUNE_SSB_FRAMES - 1) {
            CHECK(autotune_ssb_take(&st, &s, 0, 3000, false) == AUTOTUNE_STEP_COLLECTING);
        } else {
            CHECK(autotune_ssb_take(&st, &s, 0, 3000, false) == AUTOTUNE_STEP_READY);
        }
    }
    CHECK(autotune_ssb_verdict(&st, &delta) == AUTOTUNE_VERDICT_SUGGEST);
    CHECK(delta == 752);

    /* Voz no estacionaria: una voz que se corre al otro lado del dial propone el
     * desplazamiento opuesto, y esa trama NO entra (el estado nunca mezcla
     * signos). La tolerancia de la ventana (max(200, resolucion)) es mas ancha
     * que un paso de cuantizacion, asi que el ruido de jitter de una voz
     * estacionaria no la rompe: lo que la rompe es cambiar de lado. */
    autotune_ssb_reset(&st);
    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    CHECK(autotune_ssb_take(&st, &s, 0, 3000, false) == AUTOTUNE_STEP_COLLECTING);
    snap(W_96K, MODE_USB);
    energy(-1000.0f, 1500.0f, VOICE_DB);
    CHECK(autotune_ssb_take(&st, &s, 0, 3000, false) == AUTOTUNE_STEP_REJECTED);
    CHECK(st.n == 1);
    CHECK(AUTOTUNE_SSB_MIN_DELTA_HZ >= autotune_spectrum_step_hz(W_96K));

    /* Y una voz que desaparece un instante y vuelve igual: la trama que no
     * propone nada se descarta y las que si proponen siguen juntandose. La
     * abstencion no viene de perder tramas, viene de no tener una propuesta
     * (energia dentro del pasante, lejos, dos estaciones, o score bajo). */
    {
        /* La trama perdida no cuenta: hacen falta cinco propuestas validas, asi
         * que el READY llega en la sexta publicacion. */
        const autotune_step_t want[AUTOTUNE_SSB_FRAMES + 1] = {
            AUTOTUNE_STEP_COLLECTING, AUTOTUNE_STEP_COLLECTING, AUTOTUNE_STEP_REJECTED,
            AUTOTUNE_STEP_COLLECTING, AUTOTUNE_STEP_COLLECTING, AUTOTUNE_STEP_READY,
        };

        autotune_ssb_reset(&st);
        for (int i = 0; i <= AUTOTUNE_SSB_FRAMES; i++) {
            snap(W_96K, MODE_USB);
            energy(2850.0f, 1500.0f, (i == 2) ? NOISE_DB : VOICE_DB);
            CHECK(autotune_ssb_take(&st, &s, 0, 3000, false) == want[i]);
        }
    }
    CHECK(autotune_ssb_verdict(&st, &delta) == AUTOTUNE_VERDICT_SUGGEST);
    CHECK(delta == 752);
}

static void test_ssb_verdict_bounds(void) {
    autotune_ssb_state_t st;
    int32_t              delta = 0;

    autotune_ssb_reset(&st);
    st.n = AUTOTUNE_SSB_FRAMES;
    st.resolution_hz = 187.5f;
    for (int i = 0; i < AUTOTUNE_SSB_FRAMES; i++) {
        st.delta_hz[i] = AUTOTUNE_SSB_MAX_DELTA_HZ;
    }
    CHECK(autotune_ssb_verdict(&st, &delta) == AUTOTUNE_VERDICT_SUGGEST);
    CHECK(delta == AUTOTUNE_SSB_MAX_DELTA_HZ);

    /* Un desplazamiento por debajo del minimo (200 Hz, o la resolucion si es
     * mayor) no se propone nunca. */
    for (int i = 0; i < AUTOTUNE_SSB_FRAMES; i++) {
        st.delta_hz[i] = 188;
    }
    CHECK(autotune_ssb_verdict(&st, &delta) == AUTOTUNE_VERDICT_NONE);

    st.n = AUTOTUNE_SSB_FRAMES - 1;
    CHECK(autotune_ssb_verdict(&st, &delta) == AUTOTUNE_VERDICT_NONE);
    CHECK(autotune_ssb_verdict(NULL, &delta) == AUTOTUNE_VERDICT_NONE);
}

/* --- 6. ventanas de la sesion y significado de cada pulsacion --------------- */

static void test_phase_windows(void) {
    CHECK(autotune_phase_deadline(AUTOTUNE_PHASE_COLLECT, 1000) == 1000 + AUTOTUNE_COLLECT_MS);
    CHECK(autotune_phase_deadline(AUTOTUNE_PHASE_VERIFY, 1000) == 1000 + AUTOTUNE_VERIFY_MS);
    CHECK(autotune_phase_deadline(AUTOTUNE_PHASE_PREVIEW, 1000) == 1000 + AUTOTUNE_PREVIEW_MS);
    CHECK(autotune_phase_deadline(AUTOTUNE_PHASE_UNDO, 1000) == 1000 + AUTOTUNE_UNDO_MS);
    CHECK(AUTOTUNE_PREVIEW_MS == 5000);
    CHECK(AUTOTUNE_UNDO_MS == 5000);
    CHECK(AUTOTUNE_POST_WRITE_DROP == 3);

    /* Doc 5 s: a los 4999 ms sigue viva, a los 5000 vence (no hay auto-undo). */
    CHECK(!autotune_phase_expired(6000, 5999));
    CHECK(autotune_phase_expired(6000, 6000));
    CHECK(autotune_phase_expired(6000, 6001));

    CHECK(autotune_phase_measures(AUTOTUNE_PHASE_COLLECT));
    CHECK(autotune_phase_measures(AUTOTUNE_PHASE_VERIFY));
    CHECK(!autotune_phase_measures(AUTOTUNE_PHASE_PREVIEW));
    CHECK(!autotune_phase_measures(AUTOTUNE_PHASE_UNDO));
    CHECK(!autotune_phase_measures(AUTOTUNE_PHASE_IDLE));
}

static void test_press_semantics(void) {
    /* Segunda pulsacion: en PREVIEW aplica, en UNDO deshace, midiendo cancela. */
    CHECK(autotune_phase_press(AUTOTUNE_PHASE_PREVIEW) == AUTOTUNE_PRESS_APPLY);
    CHECK(autotune_phase_press(AUTOTUNE_PHASE_UNDO) == AUTOTUNE_PRESS_UNDO);
    CHECK(autotune_phase_press(AUTOTUNE_PHASE_COLLECT) == AUTOTUNE_PRESS_CANCEL);
    CHECK(autotune_phase_press(AUTOTUNE_PHASE_VERIFY) == AUTOTUNE_PRESS_CANCEL);
    CHECK(autotune_phase_press(AUTOTUNE_PHASE_IDLE) == AUTOTUNE_PRESS_CANCEL);

    /* Rollback al cancelar: SOLO la fase con una escritura sin confirmar. */
    CHECK(autotune_phase_restores_on_cancel(AUTOTUNE_PHASE_VERIFY));
    CHECK(!autotune_phase_restores_on_cancel(AUTOTUNE_PHASE_COLLECT));
    CHECK(!autotune_phase_restores_on_cancel(AUTOTUNE_PHASE_PREVIEW));
    CHECK(!autotune_phase_restores_on_cancel(AUTOTUNE_PHASE_UNDO));
    CHECK(!autotune_phase_restores_on_cancel(AUTOTUNE_PHASE_IDLE));
}

/* --- 7. composicion con la propiedad del dial (QSY externo) ---------------- */

static autotune_context_t ctx_at(int32_t freq) {
    autotune_context_t ctx;

    ctx.mode = MODE_USB;
    ctx.band_id = 7;
    ctx.vfo = 0;
    ctx.pitch = 700;
    ctx.freq = freq;
    ctx.rx = true;
    ctx.locked = false;
    ctx.rit = false;
    return ctx;
}

static void test_qsy_wins_during_preview_and_undo(void) {
    autotune_t         at;
    autotune_context_t ctx = ctx_at(START);

    /* La sesion espectral se siembra con el mismo contrato de propiedad de CW. */
    autotune_collect_start(&at, AUTOTUNE_MODE_CW, &ctx, 1000, 0);
    CHECK(autotune_owns(&at, &ctx));

    /* PREVIEW: nada escrito, el dial puede moverse; una vez que se movio, la
     * aplicacion ya no puede tocar el VFO. */
    {
        autotune_context_t qsy = ctx_at(START + 3000);

        CHECK(!autotune_owns(&at, &qsy));
    }

    /* Aplicado (segunda pulsacion): el dial es del asistente en el destino, asi
     * que el Undo puede reponer el origen. */
    autotune_note_write(&at, START + 752);
    {
        autotune_context_t applied = ctx_at(START + 752);

        CHECK(autotune_owns(&at, &applied));
    }
    /* QSY externo durante el Undo: el contexto cambio y el operador gana. */
    {
        autotune_context_t qsy = ctx_at(START + 7520);

        CHECK(!autotune_owns(&at, &qsy));
    }
    /* Y un cambio de banda/modo rompe el contexto (nunca reponer encima). */
    {
        autotune_context_t other = ctx_at(START + 752);

        other.mode = MODE_AM;
        CHECK(!autotune_owns(&at, &other));
        other = ctx_at(START + 752);
        other.band_id = 8;
        CHECK(!autotune_owns(&at, &other));
        other = ctx_at(START + 752);
        other.rx = false;
        CHECK(!autotune_owns(&at, &other));
        other = ctx_at(START + 752);
        other.locked = true;
        CHECK(!autotune_owns(&at, &other));
    }
}

static void test_target_composition(void) {
    int32_t delta = 0;

    /* La propuesta se aplica con el signo del dial: +752 Hz sube el VFO. */
    snap(W_96K, MODE_USB);
    energy(2850.0f, 1500.0f, VOICE_DB);
    CHECK(ssb_candidate(0, 3000, false, &delta));
    CHECK(autotune_target_in_band(START + delta, 14070000u, 14350000u));

    /* Y una AM a +390 Hz sube el VFO 390 Hz (la portadora queda centrada). */
    snap(W_100K, MODE_AM);
    carrier(375.0f, PEAK_DB);
    CHECK(am_candidate() == 390);
    CHECK(autotune_target_in_band(START + 390, 14070000u, 14350000u));
}

/* --- 8. la sesion AM de punta a punta (guarda, tramas nuevas, rollback) ----- */

typedef struct {
    autotune_am_state_t   am;
    autotune_frame_gate_t gate;
    int32_t               width;
    int32_t               dial;
    int32_t               start;
    int32_t               ref_dial;
    float                 ref_offset;     /* offset de la portadora EN ref_dial */
    float                 level_db;
    bool                  movable;        /* false: la portadora no sigue al dial */
    uint64_t              now;
    uint64_t              seq;
    uint64_t              epoch;
    int32_t               writes;
    int32_t               last_write;
    int32_t               post_write_skipped;
    bool                  verify;
    bool                  verified;
    bool                  rolled_back;
    bool                  failed;
} am_rig_t;

static float rig_carrier_offset(const am_rig_t *r) {
    if (!r->movable) {
        /* La portadora no se mueve con el dial (el caso que tiene que terminar en
         * rollback): se queda donde estaba en el dial de arranque. */
        return r->ref_offset + (float) (r->dial - r->start);
    }
    return r->ref_offset - (float) (r->dial - r->ref_dial);
}

static dsp_autotune_spectrum_t rig_frame(const am_rig_t *r) {
    dsp_autotune_spectrum_t f;

    memset(&f, 0, sizeof(f));
    f.sequence = r->seq;
    f.timestamp_ms = r->now;
    f.epoch = r->epoch;
    f.center_freq_hz = r->dial;
    f.mode = MODE_AM;
    f.width_hz = r->width;
    f.valid = true;
    for (int i = 0; i < AUTOTUNE_SPECTRUM_BINS; i++) {
        f.db[i] = NOISE_DB;
    }
    f.db[AUTOTUNE_SPECTRUM_CENTER +
         (int32_t) lroundf(rig_carrier_offset(r) / ((float) r->width / 800.0f))] = r->level_db;
    return f;
}

/* Un tick del coordinador AM (espeja autotune_ui.c: collect -> una escritura ->
 * verify con guarda de 300 ms y descarte de las tres publicaciones siguientes). */
static void rig_tick(am_rig_t *r) {
    dsp_autotune_spectrum_t f;

    r->now += 90;                    /* una publicacion nueva del DSP */
    r->seq++;
    f = rig_frame(r);                /* timestamp y secuencia de ESA publicacion */

    switch (autotune_frame_gate_take(&r->gate, &f)) {
        case AUTOTUNE_FRAME_BROKEN:
            r->failed = true;
            return;
        case AUTOTUNE_FRAME_TAKEN:
            break;
        default:
            if (r->verify) {
                r->post_write_skipped++;
            }
            return;
    }

    if (autotune_am_take(&r->am, &f) != AUTOTUNE_STEP_READY) {
        return;
    }

    if (!r->verify) {
        int32_t            delta = 0;
        autotune_verdict_t verdict = autotune_am_verdict(&r->am, &delta);

        if (verdict == AUTOTUNE_VERDICT_NONE) {
            r->failed = true;
            return;
        }
        if (verdict == AUTOTUNE_VERDICT_CENTERED) {
            r->verified = true;
            return;
        }
        r->dial = r->start + delta;      /* la UNICA escritura provisional */
        r->writes++;
        r->last_write = r->dial;
        autotune_am_reset(&r->am);
        r->epoch++;
        autotune_frame_gate_arm(&r->gate, r->now + AUTOTUNE_SETTLE_GUARD_MS,
                                r->seq + AUTOTUNE_POST_WRITE_DROP, r->epoch);
        r->verify = true;
        return;
    }

    if (autotune_spectrum_am_verified(&r->am)) {
        r->verified = true;
        return;
    }
    r->rolled_back = true;               /* rollback condicionado */
    r->writes++;
    r->dial = r->start;
}

static am_rig_t rig_start(int32_t width, float carrier_offset, bool movable) {
    am_rig_t r;

    memset(&r, 0, sizeof(r));
    r.width = width;
    r.start = START;
    r.dial = START;
    r.ref_dial = START;
    r.ref_offset = carrier_offset;
    r.level_db = PEAK_DB;
    r.movable = movable;
    r.now = 1000;
    r.seq = 10;
    r.epoch = 4;
    r.failed = false;
    r.last_write = START;
    autotune_am_reset(&r.am);
    autotune_frame_gate_arm(&r.gate, 0, 0, 0);
    return r;
}

static void test_am_session_converges(void) {
    am_rig_t r = rig_start(W_100K, 375.0f, true);

    for (int i = 0; i < 30 && !r.verified && !r.failed; i++) {
        rig_tick(&r);
    }
    CHECK(r.verified);
    CHECK(!r.failed);
    CHECK(!r.rolled_back);
    CHECK(r.writes == 1);                        /* una sola correccion */
    CHECK(r.last_write == START + 390);
    CHECK(r.dial == START + 390);
    CHECK(r.post_write_skipped == AUTOTUNE_POST_WRITE_DROP);
}

static void test_am_session_rolls_back_when_the_carrier_does_not_move(void) {
    am_rig_t r = rig_start(W_100K, 375.0f, false);

    for (int i = 0; i < 30 && !r.verified && !r.rolled_back && !r.failed; i++) {
        rig_tick(&r);
    }
    CHECK(r.rolled_back);
    CHECK(!r.verified);
    CHECK(r.writes == 2);                        /* provisional + rollback */
    CHECK(r.dial == START);
    CHECK(r.post_write_skipped == AUTOTUNE_POST_WRITE_DROP);
}

static void test_am_session_already_centered_writes_nothing(void) {
    am_rig_t r = rig_start(W_100K, 0.0f, true);

    for (int i = 0; i < 30 && !r.verified && !r.failed; i++) {
        rig_tick(&r);
    }
    CHECK(r.verified);
    CHECK(r.writes == 0);
    CHECK(r.dial == START);
}

static void test_am_session_wide_view_never_measures(void) {
    am_rig_t r = rig_start(W_FULL, 375.0f, true);

    for (int i = 0; i < 10 && !r.failed; i++) {
        rig_tick(&r);
    }
    CHECK(r.writes == 0);
    CHECK(r.dial == START);
}

void test_autotune_spectrum(void) {
    test_coordinates_and_resolution();
    test_measurable_limits();
    test_am_flat_and_silence();
    test_am_single_carrier();
    test_am_already_centered_and_at_the_edge();
    test_am_outside_the_window_is_not_a_carrier();
    test_am_thresholds();
    test_am_non_finite_rejects();
    test_am_too_wide_is_not_measurable();
    test_am_collect_four_frames();
    test_am_collect_inconsistent_frames();
    test_am_frames_rejected_by_the_policy();
    test_am_verdict_bounds();
    test_am_verified_needs_fresh_centered_frames();
    test_gate_freshness();
    test_gate_context_and_tx();
    test_ssb_filter_contract();
    test_ssb_usb_voice_outside_the_passband();
    test_ssb_lsb_is_mirrored();
    test_ssb_abstains();
    test_ssb_score_threshold();
    test_ssb_measurement_conditions();
    test_ssb_collect_five_frames();
    test_ssb_verdict_bounds();
    test_phase_windows();
    test_press_semantics();
    test_qsy_wins_during_preview_and_undo();
    test_target_composition();
    test_am_session_converges();
    test_am_session_rolls_back_when_the_carrier_does_not_move();
    test_am_session_already_centered_writes_nothing();
    test_am_session_wide_view_never_measures();
}

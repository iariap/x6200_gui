/*
 * Unit tests: PulseAudio capture callback (src/audio.c::read_callback).
 *
 * Covers issue #351, part 2 (contrato A): the fragment from pa_stream_peek() is memory
 * owned by PulseAudio, so the callback must
 *  - look at the return code BEFORE touching data/nbytes;
 *  - return without pa_stream_drop() on error, and silently on empty (NULL + 0);
 *  - treat NULL + nbytes>0 as a HOLE: deliver zeros, do not process, reset the pending
 *    NR state so no old audio is replayed, and drop once;
 *  - copy the PCM into a stack scratch of AUDIO_CAPTURE_RATE/10 samples and walk any
 *    positive nbytes in chunks, delivering each chunk ONCE and IN ORDER to
 *    screen_rec_put_audio_samples() and then to dsp_put_audio_samples();
 *  - process (audio_enh_process) each data chunk before both consumers, never a hole;
 *  - discard (drop) an odd nbytes with a warning instead of reinterpreting the stray
 *    byte or inventing a sample;
 *  - drop exactly once for a consumed fragment/hole, zero for error/empty, and log a
 *    failing drop without retrying;
 *  - never allocate, never call the playback/routing path (pa_stream_write,
 *    x6200_control_*) and never take the threaded-mainloop lock.
 *
 * src/audio.c is compiled BY INCLUSION: read_callback() is static and has no other
 * observable entry point, and this test has to call it with controlled peek/drop
 * responses. Everything else is the stub in tests/native/audio_callback_stubs.c.
 *
 * The fragment is allocated at its EXACT size (ASan redzones then cover the tail) and a
 * pristine copy is kept as a canary: any write to the caller's buffer - the defect of
 * the base, where audio_enh_process() ran in place on PulseAudio's memory - fails here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define _GNU_SOURCE     /* exp10f en el audio_gain_db del audio.c incluido */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_callback_stubs.h"
#include "test_framework.h"

#include "audio.c"

#define SCRATCH_SAMPLES ((size_t) (AUDIO_CAPTURE_RATE / 10))

static char fake_stream_storage;
#define FAKE_STREAM ((pa_stream *) &fake_stream_storage)

typedef struct {
    int16_t *data;
    int16_t *shadow;
    size_t   samples;
} fragment_t;

static void fragment_init(fragment_t *f, size_t samples) {
    f->samples = samples;
    f->data    = malloc(samples * sizeof(int16_t));
    f->shadow  = malloc(samples * sizeof(int16_t));

    for (size_t i = 0; i < samples; i++) {
        int32_t v = 1000 + (int32_t) ((i * 37u) % 5000u);
        int16_t s = (int16_t) ((i & 1u) ? -v : v);

        f->data[i]   = s;
        f->shadow[i] = s;
    }
}

static void fragment_free(fragment_t *f) {
    free(f->data);
    free(f->shadow);
    f->data   = NULL;
    f->shadow = NULL;
}

/* El callback no puede haber escrito el fragmento (es de PulseAudio). */
static void check_fragment_untouched(const fragment_t *f) {
    CHECK(memcmp(f->data, f->shadow, f->samples * sizeof(int16_t)) == 0);
}

/* Ni reproduccion, ni ruteo, ni lock del mainloop desde el callback. */
static void check_no_playback_or_routing(void) {
    CHECK(audio_stub_write_calls == 0);
    CHECK(audio_stub_ctrl_calls == 0);
    CHECK(audio_stub_lock_calls == 0);
}

static void check_no_delivery(void) {
    CHECK(audio_stub_process_calls == 0);
    CHECK(audio_stub_screen_calls == 0);
    CHECK(audio_stub_dsp_calls == 0);
    CHECK(audio_stub_screen_len == 0);
    CHECK(audio_stub_dsp_len == 0);
}

/* Orden de un fragmento REAL: PEEK, (PROC, SCREEN, DSP) por chunk, DROP. */
static void check_data_events(const size_t *chunks, int nchunks) {
    int e = 0;

    CHECK(!audio_stub_event_overflow);
    CHECK(audio_stub_event_count == 1 + nchunks * 3 + 1);
    CHECK(audio_stub_events[e].kind == AUDIO_EV_PEEK);
    e++;

    for (int i = 0; i < nchunks; i++) {
        CHECK(audio_stub_events[e].kind == AUDIO_EV_PROC);
        CHECK(audio_stub_events[e].samples == chunks[i]);
        e++;
        CHECK(audio_stub_events[e].kind == AUDIO_EV_SCREEN);
        CHECK(audio_stub_events[e].samples == chunks[i]);
        e++;
        CHECK(audio_stub_events[e].kind == AUDIO_EV_DSP);
        CHECK(audio_stub_events[e].samples == chunks[i]);
        e++;
    }

    CHECK(audio_stub_events[e].kind == AUDIO_EV_DROP);
}

/* Orden de un HOLE: PEEK, NR_RESET, (SCREEN, DSP) por chunk, DROP. Nada de PROC. */
static void check_hole_events(const size_t *chunks, int nchunks) {
    int e = 0;

    CHECK(!audio_stub_event_overflow);
    CHECK(audio_stub_event_count == 1 + 1 + nchunks * 2 + 1);
    CHECK(audio_stub_events[e].kind == AUDIO_EV_PEEK);
    e++;
    CHECK(audio_stub_events[e].kind == AUDIO_EV_NR_RESET);
    e++;

    for (int i = 0; i < nchunks; i++) {
        CHECK(audio_stub_events[e].kind == AUDIO_EV_SCREEN);
        CHECK(audio_stub_events[e].samples == chunks[i]);
        e++;
        CHECK(audio_stub_events[e].kind == AUDIO_EV_DSP);
        CHECK(audio_stub_events[e].samples == chunks[i]);
        e++;
    }

    CHECK(audio_stub_events[e].kind == AUDIO_EV_DROP);
}

/* Los dos consumidores reciben la MISMA cantidad de muestras y el mismo contenido. */
static void check_consumers(const int16_t *expected, size_t n) {
    CHECK(audio_stub_screen_len == n);
    CHECK(audio_stub_dsp_len == n);
    CHECK(audio_stub_screen_calls > 0);
    CHECK(audio_stub_screen_calls == audio_stub_dsp_calls);
    CHECK(memcmp(audio_stub_screen_samples, expected, n * sizeof(int16_t)) == 0);
    CHECK(memcmp(audio_stub_dsp_samples, expected, n * sizeof(int16_t)) == 0);
}

/* El stub de audio_enh_process() divide por 2 in place: es lo que ve el consumidor. */
static void expected_processed(const fragment_t *f, int16_t *out) {
    for (size_t i = 0; i < f->samples; i++) {
        out[i] = (int16_t) (f->data[i] / 2);
    }
}

/* a) ERROR: rc < 0 -> log, ni un drop, nada a los consumidores. */
static void case_peek_error(void) {
    fragment_t f;

    fragment_init(&f, 4410);
    audio_stub_reset_log();
    audio_stub_set_peek(-1, f.data, f.samples * 2);
    audio_stub_set_drop_rc(0);

    size_t allocs = audio_stub_alloc_calls;

    read_callback(FAKE_STREAM, f.samples * 2, NULL);

    CHECK(audio_stub_alloc_calls == allocs);        /* sin heap por fragmento */
    CHECK(audio_stub_peek_calls == 1);
    CHECK(audio_stub_drop_calls == 0);
    CHECK(audio_stub_error_logs == 1);
    CHECK(audio_stub_warn_logs == 0);
    check_no_delivery();
    check_fragment_untouched(&f);
    check_no_playback_or_routing();

    fragment_free(&f);
}

/* b) EMPTY: rc == 0, NULL, 0 -> retorno silencioso, sin drop. */
static void case_empty(void) {
    audio_stub_reset_log();
    audio_stub_set_peek(0, NULL, 0);
    audio_stub_set_drop_rc(0);

    read_callback(FAKE_STREAM, 0, NULL);

    CHECK(audio_stub_peek_calls == 1);
    CHECK(audio_stub_drop_calls == 0);
    CHECK(audio_stub_error_logs == 0);
    CHECK(audio_stub_warn_logs == 0);
    check_no_delivery();
    check_no_playback_or_routing();
}

/* c) HOLE: rc == 0, NULL, n > 0 -> ceros por la duracion, sin procesar, NR reseteado,
 *    drop una vez. Se ensucia antes el estado del NR con un fragmento real: despues del
 *    hole el canario tiene que estar limpio (no se reusa audio viejo). */
static void case_hole(void) {
    fragment_t     f;
    size_t         chunks[3];
    int16_t        expected[AUDIO_STUB_MAX_SAMPLES];

    fragment_init(&f, SCRATCH_SAMPLES);

    audio_stub_reset_log();
    audio_stub_set_peek(0, f.data, f.samples * 2);
    audio_stub_set_drop_rc(0);
    read_callback(FAKE_STREAM, f.samples * 2, NULL);

    CHECK(audio_stub_process_calls == 1);
    CHECK(audio_stub_screen_calls == 1);
    CHECK(audio_stub_nr_dirty);         /* el NR tiene estado pendiente */
    check_fragment_untouched(&f);

    /* Hole de 2 scratch + 137 muestras: se entrega en chunks, todo en cero. */
    chunks[0] = SCRATCH_SAMPLES;
    chunks[1] = SCRATCH_SAMPLES;
    chunks[2] = 137;

    size_t hole_samples = chunks[0] + chunks[1] + chunks[2];

    memset(expected, 0, hole_samples * sizeof(int16_t));

    audio_stub_reset_log();
    audio_stub_set_peek(0, NULL, hole_samples * 2);

    read_callback(FAKE_STREAM, hole_samples * 2, NULL);

    CHECK(audio_stub_peek_calls == 1);
    CHECK(audio_stub_drop_calls == 1);              /* una vez, al final */
    CHECK(audio_stub_nr_reset_calls == 1);
    CHECK(!audio_stub_nr_dirty);                    /* canario: no queda audio viejo */
    CHECK(audio_stub_process_calls == 0);           /* el hole no pasa por la cadena */
    CHECK(audio_stub_error_logs == 0);
    check_hole_events(chunks, 3);
    check_consumers(expected, hole_samples);        /* ceros, no el fragmento anterior */
    check_no_playback_or_routing();

    fragment_free(&f);
}

/* d) Fragmento const de 8820 bytes (4410 muestras = un scratch exacto). */
static void case_data_8820(void) {
    fragment_t f;
    size_t     chunks[1];
    int16_t    expected[AUDIO_STUB_MAX_SAMPLES];

    fragment_init(&f, 4410);
    expected_processed(&f, expected);
    chunks[0] = f.samples;

    audio_stub_reset_log();
    audio_stub_set_peek(0, f.data, 8820);
    audio_stub_set_drop_rc(0);

    read_callback(FAKE_STREAM, 8820, NULL);

    CHECK(audio_stub_peek_calls == 1);
    CHECK(audio_stub_drop_calls == 1);
    CHECK(audio_stub_process_calls == 1);
    CHECK(audio_stub_nr_reset_calls == 0);
    CHECK(audio_stub_error_logs == 0);
    check_data_events(chunks, 1);
    check_consumers(expected, f.samples);
    check_fragment_untouched(&f);
    check_no_playback_or_routing();

    fragment_free(&f);
}

/* e) Mas de 2x el scratch: chunks de min(remaining, 4410), en orden, sin perder cola. */
static void case_data_large(void) {
    fragment_t f;
    size_t     chunks[5];
    int16_t    expected[AUDIO_STUB_MAX_SAMPLES];

    fragment_init(&f, 4 * SCRATCH_SAMPLES + 137);
    expected_processed(&f, expected);

    chunks[0] = SCRATCH_SAMPLES;
    chunks[1] = SCRATCH_SAMPLES;
    chunks[2] = SCRATCH_SAMPLES;
    chunks[3] = SCRATCH_SAMPLES;
    chunks[4] = 137;

    audio_stub_reset_log();
    audio_stub_set_peek(0, f.data, f.samples * 2);
    audio_stub_set_drop_rc(0);

    read_callback(FAKE_STREAM, f.samples * 2, NULL);

    CHECK(audio_stub_peek_calls == 1);
    CHECK(audio_stub_drop_calls == 1);
    CHECK(audio_stub_process_calls == 5);
    CHECK(audio_stub_error_logs == 0);
    CHECK(audio_stub_warn_logs == 0);
    check_data_events(chunks, 5);
    check_consumers(expected, f.samples);
    check_fragment_untouched(&f);
    check_no_playback_or_routing();

    fragment_free(&f);
}

/* f) nbytes impar: fragmento PCM malformado -> aviso y drop, sin inventar la muestra. */
static void case_odd_nbytes(void) {
    fragment_t f;

    fragment_init(&f, 4410);
    audio_stub_reset_log();
    audio_stub_set_peek(0, f.data, f.samples * 2 + 1);
    audio_stub_set_drop_rc(0);

    read_callback(FAKE_STREAM, f.samples * 2 + 1, NULL);

    CHECK(audio_stub_peek_calls == 1);
    CHECK(audio_stub_warn_logs == 1);
    CHECK(audio_stub_error_logs == 0);
    CHECK(audio_stub_drop_calls == 1);      /* el fragmento se descarta entero */
    check_no_delivery();
    check_fragment_untouched(&f);
    check_no_playback_or_routing();

    fragment_free(&f);
}

/* g) Fallo de drop: se registra y NO se reintenta (un solo drop). */
static void case_drop_error(void) {
    fragment_t f;
    size_t     chunks[1];
    int16_t    expected[AUDIO_STUB_MAX_SAMPLES];

    fragment_init(&f, 4410);
    expected_processed(&f, expected);
    chunks[0] = f.samples;

    audio_stub_reset_log();
    audio_stub_set_peek(0, f.data, f.samples * 2);
    audio_stub_set_drop_rc(-1);

    read_callback(FAKE_STREAM, f.samples * 2, NULL);

    CHECK(audio_stub_drop_calls == 1);      /* sin retry dentro del callback */
    CHECK(audio_stub_error_logs == 1);
    CHECK(audio_stub_warn_logs == 0);
    check_data_events(chunks, 1);
    check_consumers(expected, f.samples);
    check_fragment_untouched(&f);
    check_no_playback_or_routing();

    fragment_free(&f);
}

void test_audio_callback(void) {
    /* El scratch es 10 ms de la tasa real de captura (4410 muestras, 8820 bytes): es el
     * tamanio que el contrato fija y el que usa el caso d). */
    CHECK(SCRATCH_SAMPLES == 4410);
    CHECK(AUDIO_CAPTURE_RATE == AUDIO_PLAY_RATE);

    case_peek_error();
    case_empty();
    case_data_8820();
    case_data_large();
    case_odd_nbytes();
    case_drop_error();
    /* El hole va al final a proposito: es el caso donde el codigo viejo desreferencia
     * NULL (pa_stream_peek con data==NULL y nbytes>0), y un SIGSEGV no deja reportar los
     * casos siguientes. Con el callback nuevo pasa como cualquier otro. */
    case_hole();
}

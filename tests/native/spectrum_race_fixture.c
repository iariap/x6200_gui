/*
 * Deliberate data-race fixture (issue #149).
 *
 * Its only job is to prove that the race detector used by the spectrum test
 * (ThreadSanitizer) really fires on THIS build/environment: a writer that
 * updates a shared buffer under a mutex (exactly what spectrum_data() does)
 * and a reader that walks the same buffer without the mutex (exactly what the
 * pre-fix spectrum_draw_cb() did).
 *
 * It is not part of the firmware and never runs against the radio: the build
 * block of tests/native/run_native_tests.sh compiles it with -fsanitize=thread
 * and REQUIRES a "data race" report (a clean exit means the detector is not
 * working and the spectrum test's TSan result would be worthless).
 *
 * The stop flag is atomic on purpose: the only unsynchronised access has to be
 * the shared buffer, so the report names the pattern under test.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>

#define RACE_FIXTURE_N 800

static float            shared[RACE_FIXTURE_N];
static pthread_mutex_t  writer_mux = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool      stop;

static void *writer(void *arg) {
    (void) arg;

    while (!atomic_load(&stop)) {
        pthread_mutex_lock(&writer_mux);

        for (int i = 0; i < RACE_FIXTURE_N; i++) {
            shared[i] += 1.0f;
        }

        pthread_mutex_unlock(&writer_mux);
    }

    return NULL;
}

static void *reader(void *arg) {
    (void) arg;

    float acc = 0.0f;

    while (!atomic_load(&stop)) {
        for (int i = 0; i < RACE_FIXTURE_N; i++) {
            acc += shared[i];
        }
    }

    /* El acumulador se consume para que el lazo no se optimice. */
    if (acc == 12345.0f) {
        printf("acc=%f\n", (double) acc);
    }

    return NULL;
}

int main(void) {
    pthread_t w, r;

    printf("fixture de carrera deliberada: escritor con mutex, lector sin mutex\n");

    pthread_create(&w, NULL, writer, NULL);
    pthread_create(&r, NULL, reader, NULL);

    /* Bounded so a broken detector still terminates: the loop is short and the
     * race shows up in the first iterations. */
    for (volatile int spin = 0; spin < 2000000; spin++) {
    }

    atomic_store(&stop, true);

    pthread_join(w, NULL);
    pthread_join(r, NULL);

    printf("sin reporte del detector\n");

    return 0;
}

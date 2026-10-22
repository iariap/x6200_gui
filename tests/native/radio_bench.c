/*
 * Banco host antes/despues del fail-slow de radio_thread (issue #219).
 *
 * Corre el lazo REAL de src/radio.c (por inclusion, RADIO_C_PATH) en una ventana
 * de N segundos con x6200_flow_read() devolviendo true, o sea radio_tick()
 * devolviendo false: el caso en el que la version con el defecto giraba sin
 * dormir. El mismo binario se compila dos veces, contra el radio.c de la base y
 * contra el del fix; la unica diferencia es la version del lazo.
 *
 * Se mide por ventana: iteraciones (llamadas a x6200_flow_read), sleeps, CPU de
 * proceso (getrusage: utime+stime) y tiempo de pared. CPU% es sobre UN nucleo
 * (puede pasar de 100 si el proceso tuviera mas hilos; aca solo corre el hilo de
 * radio + el principal que espera).
 *
 * Uso: radio_bench <segundos>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include "radio_stubs.h"

#ifndef RADIO_C_PATH
#error "definir RADIO_C_PATH al src/radio.c stageado"
#endif

#include RADIO_C_PATH

static x6200_flow_t test_pack;

static uint64_t wall_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t) ts.tv_sec * 1000u + (uint64_t) ts.tv_nsec / 1000000u;
}

static void *thread_main(void *arg) {
    (void) arg;

    radio_thread(NULL);

    return NULL;
}

int main(int argc, char **argv) {
    int seconds = (argc > 1) ? atoi(argv[1]) : 5;

    if (seconds <= 0) {
        seconds = 5;
    }

    radio_stub_reset();
    radio_stub_clock        = RADIO_STUB_CLOCK_MONOTONIC;
    radio_stub_flow_ok      = true;   /* radio_tick() false: el caso del busy-spin */
    radio_stub_exit_on_stop = true;

    now_time  = 0;
    prev_time = 0;
    idle_time = 0;
    state     = RADIO_RX;
    pack      = &test_pack;
    memset(&test_pack, 0, sizeof(test_pack));

    struct rusage r0;
    struct rusage r1;

    getrusage(RUSAGE_SELF, &r0);
    uint64_t t0 = wall_ms();

    pthread_t th;

    if (pthread_create(&th, NULL, thread_main, NULL) != 0) {
        fprintf(stderr, "no se pudo crear el hilo de radio\n");

        return 1;
    }

    struct timespec req = { .tv_sec = seconds, .tv_nsec = 0 };

    nanosleep(&req, NULL);

    radio_stub_stop = true;
    pthread_join(th, NULL);

    uint64_t t1 = wall_ms();

    getrusage(RUSAGE_SELF, &r1);

    double wall = (double) (t1 - t0);

    long long u0 = (long long) r0.ru_utime.tv_sec * 1000000LL + r0.ru_utime.tv_usec;
    long long u1 = (long long) r1.ru_utime.tv_sec * 1000000LL + r1.ru_utime.tv_usec;
    long long s0 = (long long) r0.ru_stime.tv_sec * 1000000LL + r0.ru_stime.tv_usec;
    long long s1 = (long long) r1.ru_stime.tv_sec * 1000000LL + r1.ru_stime.tv_usec;

    double cpu_ms = (double) ((u1 - u0) + (s1 - s0)) / 1000.0;

    printf("%s\t%.0f\t%llu\t%llu\t%.1f\t%.2f\n",
           argv[0],
           wall,
           (unsigned long long) radio_stub_flow_reads,
           (unsigned long long) radio_stub_sleep_calls,
           cpu_ms,
           100.0 * cpu_ms / wall);

    return 0;
}

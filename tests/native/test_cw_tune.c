/*
 * Native test: cw_tune_ui.c (issues #131).
 *
 * cw_put_audio_samples() corre en el hilo de audio (PulseAudio mainloop, via
 * dsp_put_audio_samples) y alimenta el widget de sintonia de CW:
 *
 *   - consulta cw_tune_ui_visible() en cada bloque de audio, y
 *   - cw_tune_set_freq() cuando detecta un tono.
 *
 * Las dos eran LVGL desde ese hilo (lv_obj_has_flag y lv_event_send): eso
 * corrompe la lista de eventos de LVGL y es el SIGSEGV del #131. El test fija
 * que desde un hilo ajeno las dos dejan el trabajo en el scheduler y no tocan
 * LVGL; el cuerpo corre en el hilo de GUI.
 *
 * RED antes del arreglo (las dos tocando LVGL en el hilo que llama):
 *   cw_tune_stub_lvgl_foreign_calls() > 0, scheduler_pending() == 0 y
 *   cw_tune_stub_refresh_foreign() > 0.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <pthread.h>
#include <stdint.h>

#include "cw_tune_ui.h"
#include "cw_tune_stubs.h"
#include "scheduler.h"
#include "test_framework.h"

#include "radio.h"

static lv_obj_t *screen;
static bool      inited;

/* Corre el cuerpo en OTRO hilo, como el feed de audio en la radio. */
static void *set_freq_thread(void *arg) {
    cw_tune_set_freq((float) (intptr_t) arg);
    return NULL;
}

static void set_freq_from_audio_thread(float hz) {
    pthread_t t;

    CHECK(pthread_create(&t, NULL, set_freq_thread, (void *) (intptr_t) hz) == 0);
    CHECK(pthread_join(t, NULL) == 0);
}

static void *visible_thread(void *arg) {
    *(bool *) arg = cw_tune_ui_visible();
    return NULL;
}

static bool visible_from_audio_thread(void) {
    bool      result = false;
    pthread_t t;

    CHECK(pthread_create(&t, NULL, visible_thread, &result) == 0);
    CHECK(pthread_join(t, NULL) == 0);

    return result;
}

static void begin(int32_t mode) {
    cw_tune_stub_reset();
    scheduler_thread_init();
    /* Desde aca, "el hilo de LVGL" es el hilo del test (el de GUI). */
    cw_tune_stub_lvgl_thread(pthread_self());

    if (screen == NULL) {
        screen = lv_obj_create(NULL);
    }

    cw_tune_stub_mode(mode);

    if (!inited) {
        cw_tune_init(screen);       /* crea el widget y fija la visibilidad del modo */
        inited = true;
    } else {
        cw_tune_stub_delayed_notify();  /* el observer de modo, como el lazo de GUI */
    }

    cw_tune_stub_reset();
}

/* --- 1. el pedido de frecuencia se difiere --------------------------------- */

static void test_set_freq_from_audio_thread_is_deferred(void) {
    begin(x6200_mode_cw);

    set_freq_from_audio_thread(5.0f);

    /* Nada de LVGL en el hilo de audio; el trabajo quedo encolado. */
    CHECK(cw_tune_stub_lvgl_calls() == 0);
    CHECK(cw_tune_stub_lvgl_foreign_calls() == 0);
    CHECK(cw_tune_stub_refresh_events() == 0);
    CHECK(scheduler_pending() == 1);

    scheduler_work();

    /* El refresco corrio, y corrio en el hilo de GUI. */
    CHECK(cw_tune_stub_refresh_events() == 1);
    CHECK(cw_tune_stub_refresh_foreign() == 0);
    CHECK(cw_tune_stub_lvgl_foreign_calls() == 0);
    CHECK(scheduler_pending() == 0);
}

/* --- 2. consultar la visibilidad no toca LVGL ------------------------------ */

static void test_visible_from_audio_thread_does_not_touch_lvgl(void) {
    begin(x6200_mode_cw);

    CHECK(visible_from_audio_thread() == true);

    CHECK(cw_tune_stub_lvgl_calls() == 0);
    CHECK(cw_tune_stub_lvgl_foreign_calls() == 0);
    CHECK(scheduler_pending() == 0);
}

/* --- 3. fuera de CW el widget no esta visible ------------------------------ */

static void test_not_visible_outside_cw(void) {
    begin(x6200_mode_usb);

    CHECK(visible_from_audio_thread() == false);
    CHECK(cw_tune_stub_lvgl_foreign_calls() == 0);

    /* Con el modo de vuelta en CW, visible sin tocar LVGL desde el hilo ajeno. */
    begin(x6200_mode_cw);

    CHECK(visible_from_audio_thread() == true);
    CHECK(cw_tune_stub_lvgl_foreign_calls() == 0);
}

/* --- 4. varios pedidos se coalescen en un solo trabajo --------------------- */

static void test_pending_requests_coalesce(void) {
    begin(x6200_mode_cw);

    set_freq_from_audio_thread(1.0f);
    set_freq_from_audio_thread(2.0f);
    set_freq_from_audio_thread(3.0f);

    /* Un solo trabajo encolado mientras haya uno pendiente. */
    CHECK(scheduler_pending() == 1);
    CHECK(cw_tune_stub_lvgl_foreign_calls() == 0);

    scheduler_work();

    CHECK(scheduler_pending() == 0);
    CHECK(cw_tune_stub_refresh_events() == 1);
    CHECK(cw_tune_stub_refresh_foreign() == 0);
}

void test_cw_tune(void) {
    test_set_freq_from_audio_thread_is_deferred();
    test_visible_from_audio_thread_does_not_touch_lvgl();
    test_not_visible_outside_cw();
    test_pending_requests_coalesce();
}

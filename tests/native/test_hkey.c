/*
 * Native test: hkey_put() (issues #131).
 *
 * El operador cambia banda y filtro con las teclas del panel, y esas teclas
 * llegan por `radio_tick()`: `hkey_put(pack->hkey)` corre en el hilo de radio,
 * no en el de LVGL. El test fija dos cosas, las dos con el modulo real
 * staged contra stubs que registran el hilo que los llama:
 *
 *   1. Desde un hilo ajeno, hkey_put() NO toca LVGL ni encola eventos: solo
 *      deja el trabajo en el scheduler (que drena el hilo de LVGL). Esto cubre
 *      las lecturas de grupo (lv_group_get_editing / lv_group_get_focused), las
 *      tres operaciones de timer (lv_timer_create/del/set_repeat_count) y el
 *      lv_timer_* que hace backlight_tick().
 *   2. La maquina de estados de la tecla no cambia: press/long/release, teclas
 *      numericas con y sin edicion, y el orden de los eventos que se encolan.
 *
 * RED antes del arreglo (hkey_put haciendo LVGL en el hilo que llama):
 *   hkey_stub_lvgl_foreign_calls() > 0 y scheduler_pending() == 0.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <pthread.h>
#include <stdint.h>

#include "hkey.h"
#include "hkey_stubs.h"
#include "scheduler.h"
#include "test_framework.h"

#include <aether_radio/x6200_control/low/flow.h>

static void *put_thread(void *arg) {
    hkey_put((uint32_t) (uintptr_t) arg);
    return NULL;
}

/* Llama hkey_put() desde OTRO hilo, como radio_tick() en la radio. */
static void put_from_radio_thread(uint32_t key) {
    pthread_t t;

    CHECK(pthread_create(&t, NULL, put_thread, (void *) (uintptr_t) key) == 0);
    CHECK(pthread_join(t, NULL) == 0);
}

static void begin(void) {
    /* Deja el modulo en reposo antes de limpiar los contadores: un release
     * procesado cancela el timer de long-press y baja el flag de armado, asi
     * cada caso arranca del mismo estado sin tocar los estaticos del modulo. */
    hkey_put(0);
    scheduler_work();
    hkey_stub_reset();
    /* Desde aca, "el hilo de LVGL" es el hilo del test. */
    hkey_stub_lvgl_thread(pthread_self());
}

/* --- 1. el invariante de hilos -------------------------------------------- */

static void test_puts_from_radio_thread_are_deferred(void) {
    begin();

    put_from_radio_thread(X6200_HKEY_F1);

    /* Nada de LVGL y ningun evento se disparo en el hilo de radio. */
    CHECK(hkey_stub_lvgl_calls() == 0);
    CHECK(hkey_stub_lvgl_foreign_calls() == 0);
    CHECK(hkey_stub_events() == 0);
    CHECK(hkey_stub_events_foreign() == 0);
    CHECK(hkey_stub_backlight_ticks() == 0);

    /* El trabajo quedo encolado para el hilo de LVGL. */
    CHECK(scheduler_pending() == 1);

    scheduler_work();

    /* Todo corrio, y todo corrio en el hilo de LVGL. */
    CHECK(hkey_stub_lvgl_foreign_calls() == 0);
    CHECK(hkey_stub_events_foreign() == 0);
    CHECK(hkey_stub_events() == 1);
    CHECK(hkey_stub_backlight_ticks() == 1);
    CHECK(hkey_stub_backlight_foreign() == 0);
    CHECK(hkey_stub_timers_live() == 1);
    CHECK(scheduler_pending() == 0);

    event_hkey_t ev = hkey_stub_last_event();

    CHECK(ev.key == HKEY_F1);
    CHECK(ev.state == HKEY_PRESS);

    /* El long-press se arma en el hilo de LVGL (1000 ms, una sola vez). */
    CHECK(hkey_stub_last_timer_period() == 1000);
    CHECK(hkey_stub_last_timer_repeat() == 1);
}

/* --- 2. release y long press ---------------------------------------------- */

static void test_press_then_release(void) {
    begin();

    put_from_radio_thread(X6200_HKEY_F1);
    scheduler_work();

    unsigned lvgl_before = hkey_stub_lvgl_calls();

    put_from_radio_thread(0);

    /* El release tampoco toca LVGL desde el hilo de radio. */
    CHECK(hkey_stub_lvgl_calls() == lvgl_before);
    CHECK(scheduler_pending() == 1);

    scheduler_work();

    CHECK(hkey_stub_events() == 2);

    event_hkey_t ev = hkey_stub_last_event();

    CHECK(ev.key == HKEY_F1);
    CHECK(ev.state == HKEY_RELEASE);

    /* El release cancela el timer de long-press. */
    CHECK(hkey_stub_timers_live() == 0);
    CHECK(hkey_stub_timers_deleted() == 1);
    CHECK(hkey_stub_timers_double_del() == 0);
}

static void test_long_press(void) {
    begin();

    put_from_radio_thread(X6200_HKEY_F1);
    scheduler_work();

    /* El timer del long-press vence (lo corre LVGL). */
    CHECK(hkey_stub_timer_fire());

    CHECK(hkey_stub_events() == 2);

    event_hkey_t ev = hkey_stub_last_event();

    CHECK(ev.key == HKEY_F1);
    CHECK(ev.state == HKEY_LONG);

    put_from_radio_thread(0);
    scheduler_work();

    CHECK(hkey_stub_events() == 3);

    ev = hkey_stub_last_event();

    CHECK(ev.key == HKEY_F1);
    CHECK(ev.state == HKEY_LONG_RELEASE);
    CHECK(hkey_stub_timers_live() == 0);

    /* El timer ya vencio: el release no lo vuelve a borrar. */
    CHECK(hkey_stub_timers_deleted() == 1);
    CHECK(hkey_stub_timers_double_del() == 0);
}

/* --- 3. el cero sin press no encola trabajo ------------------------------- */

static void test_zero_without_press_is_noop(void) {
    begin();

    put_from_radio_thread(0);

    CHECK(hkey_stub_lvgl_calls() == 0);
    CHECK(scheduler_pending() == 0);

    put_from_radio_thread(0);
    CHECK(scheduler_pending() == 0);

    scheduler_work();

    CHECK(hkey_stub_events() == 0);
    CHECK(hkey_stub_lvgl_foreign_calls() == 0);
}

/* --- 4. teclas numericas --------------------------------------------------- */

static void test_digit_key_without_editing(void) {
    begin();
    hkey_stub_set_editing(false);

    put_from_radio_thread(X6200_HKEY_2);

    CHECK(hkey_stub_lvgl_calls() == 0);
    CHECK(scheduler_pending() == 1);

    scheduler_work();

    /* Fuera de edicion la tecla va directo al grupo como caracter. */
    CHECK(hkey_stub_keys() == 1);
    CHECK(hkey_stub_last_key() == '2');
    CHECK(hkey_stub_keys_foreign() == 0);
    CHECK(hkey_stub_events() == 0);
    CHECK(hkey_stub_timers_live() == 0);

    put_from_radio_thread(0);
    scheduler_work();

    CHECK(hkey_stub_events() == 0);
    CHECK(hkey_stub_timers_live() == 0);
}

static void test_digit_key_while_editing(void) {
    begin();
    hkey_stub_set_editing(true);

    put_from_radio_thread(X6200_HKEY_2);
    scheduler_work();

    CHECK(hkey_stub_keys() == 0);
    CHECK(hkey_stub_events() == 1);

    event_hkey_t ev = hkey_stub_last_event();

    CHECK(ev.key == HKEY_2);
    CHECK(ev.state == HKEY_PRESS);
    CHECK(hkey_stub_timers_live() == 1);

    put_from_radio_thread(0);
    scheduler_work();

    ev = hkey_stub_last_event();

    CHECK(ev.key == HKEY_2);
    CHECK(ev.state == HKEY_RELEASE);
    CHECK(hkey_stub_timers_live() == 0);
}

/* --- 5. el orden de la cola es el orden de las teclas --------------------- */

static void test_pending_keys_keep_order(void) {
    begin();

    /* Tres transiciones antes de que el hilo de LVGL drene la cola. */
    put_from_radio_thread(X6200_HKEY_F1);
    put_from_radio_thread(X6200_HKEY_F2);
    put_from_radio_thread(0);

    CHECK(scheduler_pending() == 3);
    CHECK(hkey_stub_events() == 0);

    scheduler_work();

    /* press(F1) y despues el release de la tecla vigente (F2), en ese orden. */
    CHECK(hkey_stub_events() == 2);
    CHECK(hkey_stub_events_foreign() == 0);
    CHECK(hkey_stub_lvgl_foreign_calls() == 0);
    CHECK(hkey_stub_timers_live() == 0);
    CHECK(hkey_stub_timers_double_del() == 0);
}

void test_hkey(void) {
    /* Primero el cero sin press: el flag de armado del modulo arranca en false.
     * Los tests que siguen cierran siempre con un release ya procesado. */
    test_zero_without_press_is_noop();
    test_puts_from_radio_thread_are_deferred();
    test_press_then_release();
    test_long_press();
    test_digit_key_without_editing();
    test_digit_key_while_editing();
    test_pending_keys_keep_order();
}

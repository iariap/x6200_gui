/*
 * Fixture API of the WiFi idle-pump native stage (issue #218, F-06).
 *
 * The stage compiles the REAL src/wifi.cpp (staged next to the minimal stubs of
 * tests/native/stubs_wifi/) and drives it over this fixture instead of over a radio:
 *
 *   - a timer engine that mirrors lvgl/src/misc/lv_timer.c, with a virtual clock
 *     (wifi_fx_run() advances 1 ms per step and fires what is due): asi el periodo que el
 *     modulo le pone al timer se convierte en un numero de vueltas medible;
 *   - a GLib main-loop pump that counts `g_main_context_iteration()` calls and dispatches
 *     the sources the NetworkManager double posts (a scan request, a late signal), the way
 *     the real default context would;
 *   - recorders of the side effects the operator sees: the WiFi pin, `params.wifi_enabled`,
 *     the LVGL message bus and the text of msg_update_text_fmt().
 *
 * Nada de esto sustituye a la radio: la energia real, el estado del pin fisico y la WiFi
 * verdadera quedan PENDING_HARDWARE (docs/evidencia/2026-10-22-next218/README.md).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "NetworkManager.h"
#include "lvgl/lvgl.h"
#include "params/params.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- estado del fixture ---------------------------------------------------- */
/* Deja el fixture en cero (timers, reloj, contadores, fuentes, pin, mensajes y
 * params.wifi_enabled.x = false). No toca los estaticos del modulo: eso lo hace el test. */
void wifi_fx_reset(void);

/* --- timers ---------------------------------------------------------------- */
uint32_t wifi_fx_tick(void);
uint32_t wifi_fx_timers_live(void);
uint32_t wifi_fx_timers_created(void);
uint32_t wifi_fx_timers_deleted(void);
uint32_t wifi_fx_timer_period(const lv_timer_t *timer);
/* Adelanta el reloj hasta el vencimiento de `timer` (ejecutando lo que venza), para que la
 * ventana siguiente empiece alineada con ese timer: el conteo de W ms vale W/periodo. */
void     wifi_fx_align(lv_timer_t *timer);
/* Avanza `window_ms` ms y devuelve cuantas veces corrio el pump de GLib en la ventana. */
uint32_t wifi_fx_run(uint32_t window_ms);

/* --- pump de GLib ---------------------------------------------------------- */
uint32_t wifi_fx_pump_iterations(void);
uint32_t wifi_fx_pump_quits(void);
uint32_t wifi_fx_async_pending(void);
uint32_t wifi_fx_async_dispatched(void);
/* Encola una fuente en el contexto (lo que hace libnm cuando una async call termina). */
void     wifi_fx_post(void (*cb)(void *), void *user_data);

/* --- NetworkManager -------------------------------------------------------- */
/* false (default): nm_client_new() falla, el modulo queda sin adaptador NM. */
void      wifi_fx_nm_present(bool present);
void      wifi_fx_nm_last_scan(uint64_t value);
NMDevice *wifi_fx_nm_device(void);
uint32_t  wifi_fx_signal_connects(const char *signal_name);
uint32_t  wifi_fx_signal_connect_null(void);

/* --- radio / params / mensajes --------------------------------------------- */
int      wifi_fx_gpio_last_pin(void);
int      wifi_fx_gpio_last_value(void);
uint32_t wifi_fx_gpio_writes(void);
/* Escrituras de nivel 0 sobre el pin de WiFi: la radio encendida. */
uint32_t wifi_fx_gpio_power_on_writes(void);
uint32_t wifi_fx_params_bool_set_calls(void);
uint32_t wifi_fx_log_calls(int level);
uint32_t wifi_fx_msg_sent(void);
uint32_t wifi_fx_msg_state_changed(void);
const char *wifi_fx_last_msg_text(void);

#ifdef __cplusplus
}
#endif

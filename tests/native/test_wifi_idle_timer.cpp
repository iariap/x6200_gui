/*
 * Native test of the WiFi idle GLib pump (issue #218, F-06; hijo 2/6 de la epica #216).
 *
 * Que se prueba: que el modulo conserve EXACTAMENTE un loop_timer por proceso, que su
 * periodo sea el de la radio encendida (30 ms) cuando la WiFi esta ON y el de la radio
 * apagada (250 ms) cuando esta OFF, y que el pump de GLib -el que despacha las callbacks
 * async de NetworkManager- siga vivo en los dos estados y despues de todas las
 * transiciones. El callback de LVGL y la red no se tocan: lo unico que cambia es cuantas
 * veces por segundo se lo llama.
 *
 * Como: se incluye el src/wifi.cpp REAL (staged, con sus estaticos a la vista) y se lo
 * corre sobre el fixture de tests/native/wifi_idle_stubs.cpp (motor de timers que espeja
 * lvgl/src/misc/lv_timer.c + pump de GLib que cuenta sus vueltas), con un reloj virtual:
 * los numeros son exactos y reproducibles, no una estimacion.
 *
 * Lo que se mide (escenario "WiFi ociosa", sin scan pendiente): vueltas del pump en una
 * ventana de 10 s. Con el periodo de la radio encendida son 10000/30 = 333 (33,3/s) y con
 * el de la apagada 10000/250 = 40 (4,0/s). Es SOLO este timer: el total del sistema (tick
 * de LVGL, DSP, pantalla) no lo mide este stage, y el ahorro en watts no se afirma sin
 * medidor (PENDING_HARDWARE).
 *
 * El contrato de la epica pide ademas la constancia de que NetworkManager sigue igual:
 * conexion/reconexion/estado visible siguen andando con el periodo nuevo, una callback
 * tardia (una desconexion que llega despues del OFF) se sigue despachando y nunca
 * reenciende la radio ni el pin.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

extern "C" {
#include "test_framework.h"
}

#include "wifi_idle_fixture.h"

/* La unidad bajo prueba: el wifi.cpp staged. Sus estaticos (loop_timer, scan_timer,
 * device, scanning, last_scan) quedan visibles para el test, como en test_remote.cpp. */
#include "wifi.cpp"

/* Periodos del contrato: los mismos valores que el issue #218 fija. */
static const uint32_t PERIOD_ON_MS = 30;
static const uint32_t PERIOD_OFF_MS = 250;
/* Ventana de la medicion: 10 s ociosos. */
static const uint32_t WINDOW_MS = 10000;

static uint32_t g_off_wakeups;
static uint32_t g_on_wakeups;

/* Deja fixture y modulo en el estado previo al setup (como al arrancar la GUI). */
static void module_reset(void) {
    wifi_fx_reset();

    loop = NULL;
    client = NULL;
    device = NULL;
    loop_timer = NULL;
    scan_timer = NULL;
    scanning = false;
    last_scan = 0;
    status = WIFI_OFF;
}

/* Setup como el de la GUI: params.wifi_enabled manda. `nm_present` decide si NetworkManager
 * contesta con el wlan0 o si el modulo arranca sin adaptador. */
static void module_setup(bool wifi_enabled, bool nm_present) {
    module_reset();
    wifi_fx_nm_present(nm_present);
    params.wifi_enabled.x = wifi_enabled;
    wifi_power_setup();
}

/* Una ventana ociosa medida desde un vencimiento del loop_timer: W ms / periodo vueltas. */
static uint32_t idle_wakeups(uint32_t window_ms) {
    wifi_fx_align(loop_timer);

    return wifi_fx_run(window_ms);
}

/* Adelanta el reloj (1 ms por paso) hasta la proxima vuelta del pump y devuelve cuantos ms
 * tardo: es la latencia con la que una callback async de NM se despacha en ese estado. */
static uint32_t pump_once(uint32_t max_ms) {
    uint32_t before = wifi_fx_pump_iterations();
    uint32_t waited = 0;

    while (waited < max_ms) {
        wifi_fx_run(1);
        waited++;

        if (wifi_fx_pump_iterations() != before) {
            break;
        }
    }

    return waited;
}

static void print_measurement(const char *state, uint32_t period_ms, uint32_t wakeups) {
    printf("  %-3s (periodo %3u ms): %3u vueltas del pump en %u ms (%.1f/s)\n", state, period_ms, wakeups,
           WINDOW_MS, (double)wakeups * 1000.0 / (double)WINDOW_MS);
}

/* --- instrumento: los dos numeros antes de cualquier asercion --------------- */
/* Corre primero y sin CHECKs para que una corrida roja (por ejemplo la del codigo anterior
 * a #218) igual imprima lo medido: es la mitad "antes" del A/B. */
static void measure_idle_wakeups(void) {
    printf("\n== medicion: vueltas del pump de GLib con la WiFi ociosa (%u ms de reloj virtual) ==\n",
           WINDOW_MS);
    printf("   pump de GLib = g_main_context_iteration(), el que despacha las callbacks async de\n");
    printf("   NetworkManager. Solo este timer; el total del sistema no se mide aca.\n");

    module_setup(false, false);
    g_off_wakeups = idle_wakeups(WINDOW_MS);
    print_measurement("OFF", wifi_fx_timer_period(loop_timer), g_off_wakeups);

    wifi_power_on();
    g_on_wakeups = idle_wakeups(WINDOW_MS);
    print_measurement("ON", wifi_fx_timer_period(loop_timer), g_on_wakeups);

    printf("  timers vivos: %u (creados %u, borrados %u)\n", wifi_fx_timers_live(), wifi_fx_timers_created(),
           wifi_fx_timers_deleted());
}

/* --- contrato: un solo timer, periodo por estado --------------------------- */
static void test_off_at_start(void) {
    printf("\n== OFF al inicio: un timer, periodo %u ms, pump lento pero vivo ==\n", PERIOD_OFF_MS);

    module_setup(false, false);

    CHECK(loop_timer != NULL);
    CHECK(scan_timer == NULL);
    CHECK(wifi_fx_timers_live() == 1);
    CHECK(wifi_fx_timers_created() == 1);
    CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_OFF_MS);

    /* La radio queda apagada como antes: pin en 1, params en false, estado visible OFF. */
    CHECK(wifi_fx_gpio_last_pin() == x6200_pin_wifi);
    CHECK(wifi_fx_gpio_last_value() == 1);
    CHECK(wifi_fx_gpio_power_on_writes() == 0);
    CHECK(params.wifi_enabled.x == false);
    CHECK(wifi_get_status() == WIFI_OFF);
    CHECK(wifi_scanning() == false);

    /* El pump sigue existiendo: la ventana ociosa lo despierta 10000/250 veces. */
    g_off_wakeups = idle_wakeups(WINDOW_MS);
    CHECK(g_off_wakeups == WINDOW_MS / PERIOD_OFF_MS);

    /* El timer sigue siendo uno solo despues de la ventana y nada lo cierra. */
    CHECK(wifi_fx_timers_live() == 1);
    CHECK(wifi_fx_pump_quits() == 0);
}

static void test_on_at_start(void) {
    printf("\n== ON al inicio: un timer, periodo %u ms, ritmo de siempre ==\n", PERIOD_ON_MS);

    module_setup(true, false);

    CHECK(loop_timer != NULL);
    CHECK(wifi_fx_timers_live() == 1);
    CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_ON_MS);

    /* Encender la radio no cambia lo que el modulo ya hacia: pin en 0, params en true. */
    CHECK(wifi_fx_gpio_last_value() == 0);
    CHECK(wifi_fx_gpio_power_on_writes() == 1);
    CHECK(params.wifi_enabled.x == true);
    /* Sin adaptador NM el estado visible sigue siendo STARTING (el modulo espera device). */
    CHECK(wifi_get_status() == WIFI_STARTING);

    g_on_wakeups = idle_wakeups(WINDOW_MS);
    CHECK(g_on_wakeups == WINDOW_MS / PERIOD_ON_MS);

    /* La radio encendida sigue levantando el pump al ritmo del timer de siempre, y el
     * estado ocioso de la apagada es mas barato. */
    CHECK(g_on_wakeups > g_off_wakeups);
}

static void test_100_cycles(void) {
    printf("\n== 100 ciclos ON/OFF: un timer siempre, sin duplicados ni leaks ==\n");

    module_setup(false, false);
    CHECK(wifi_fx_timers_created() == 1);

    for (int i = 0; i < 100; i++) {
        wifi_power_off();
        CHECK(wifi_fx_timers_live() == 1);
        CHECK(loop_timer != NULL);
        CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_OFF_MS);

        wifi_power_on();
        CHECK(wifi_fx_timers_live() == 1);
        CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_ON_MS);
    }

    /* El modulo creo un solo timer en toda la corrida (el del setup) y nunca lo borro: el
     * contrato pide conservarlo para no perder callbacks. */
    CHECK(wifi_fx_timers_created() == 1);
    CHECK(wifi_fx_timers_deleted() == 0);
    CHECK(wifi_fx_timers_live() == 1);

    /* Cambiar el periodo no toca el pin: una escritura por transicion, como antes. */
    CHECK(wifi_fx_gpio_writes() == 1 + 200);
    CHECK(wifi_fx_gpio_power_on_writes() == 100);

    wifi_power_off();
    CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_OFF_MS);
}

/* --- scan pendiente al OFF y reanudacion del scan -------------------------- */
static void test_scan_pending_at_off(void) {
    printf("\n== scan pendiente al OFF: se cancela el scan, el pump no se toca ==\n");

    module_setup(true, true);
    CHECK(client != NULL);
    CHECK(device != NULL);
    CHECK(wifi_get_status() == WIFI_DISCONNECTED);
    CHECK(wifi_fx_signal_connects("device-added") == 1);
    CHECK(wifi_fx_signal_connects("state-changed") == 1);

    /* Scan pedido a NM: la respuesta llega por el pump, no en la misma vuelta. */
    wifi_start_scan();
    CHECK(wifi_fx_async_pending() == 1);
    CHECK(wifi_scanning() == false);

    CHECK(pump_once(PERIOD_ON_MS) == PERIOD_ON_MS);
    CHECK(wifi_fx_async_dispatched() == 1);
    CHECK(wifi_scanning() == true);
    CHECK(scan_timer != NULL);
    CHECK(wifi_fx_timers_live() == 2);   /* loop_timer + scan_timer */
    CHECK(wifi_fx_pump_quits() == 1);    /* el modulo cierra su espera async */

    /* Apagar la radio cancela el scan (comportamiento de siempre) y deja el pump con el
     * periodo de OFF: no se borra el loop_timer ni se cierra GLib. */
    wifi_power_off();
    CHECK(wifi_scanning() == false);
    CHECK(scan_timer == NULL);
    CHECK(wifi_fx_timers_deleted() == 1);
    CHECK(wifi_fx_timers_live() == 1);
    CHECK(loop_timer != NULL);
    CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_OFF_MS);
    CHECK(wifi_get_status() == WIFI_OFF);

    /* Y el pump despacha igual con la radio apagada. */
    CHECK(idle_wakeups(WINDOW_MS) == WINDOW_MS / PERIOD_OFF_MS);

    /* Repower ON: NM vuelve a anunciar el device (device-added) y el scan se reanuda y
     * termina por su timer de 500 ms, como antes. */
    wifi_power_on();
    CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_ON_MS);
    device_added_sig_cb(client, (GObject *)wifi_fx_nm_device(), NULL);
    CHECK(device != NULL);

    wifi_start_scan();
    CHECK(pump_once(PERIOD_ON_MS) <= PERIOD_ON_MS);
    CHECK(wifi_scanning() == true);
    CHECK(wifi_fx_timers_live() == 2);

    wifi_fx_nm_last_scan(2000);   /* NM reporta otro last_scan: el scan termino */
    wifi_fx_run(500);
    CHECK(wifi_scanning() == false);
    CHECK(scan_timer == NULL);
    CHECK(wifi_fx_timers_deleted() == 2);
    CHECK(wifi_fx_timers_live() == 1);
    CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_ON_MS);
    CHECK(wifi_get_status() == WIFI_DISCONNECTED);

    /* Una vuelta ociosa con la radio encendida sigue midiendo el periodo de ON. */
    CHECK(idle_wakeups(WINDOW_MS) == WINDOW_MS / PERIOD_ON_MS);
}

/* --- desconexion tardia ---------------------------------------------------- */
static void test_late_callback_after_off(void) {
    printf("\n== desconexion tardia: una callback post-OFF se despacha y no reenciende nada ==\n");

    uint32_t late_calls = 0;

    module_setup(false, false);

    /* Lo que libnm deja en el contexto cuando su async call termina despues del OFF: el
     * modulo tiene que seguir despachandola. */
    wifi_fx_post([](void *user_data) { (*(uint32_t *)user_data)++; }, &late_calls);
    CHECK(wifi_fx_async_pending() == 1);
    CHECK(late_calls == 0);

    /* Se despacha en la primera vuelta del pump, que con la radio apagada tarda 250 ms. */
    CHECK(pump_once(PERIOD_OFF_MS) == PERIOD_OFF_MS);
    CHECK(late_calls == 1);
    CHECK(wifi_fx_async_pending() == 0);
    CHECK(wifi_fx_async_dispatched() == 1);

    /* Y despues de la callback tardia la radio sigue apagada: ni el pin ni params cambian
     * mientras el pump siga corriendo. */
    CHECK(wifi_fx_run(WINDOW_MS) == WINDOW_MS / PERIOD_OFF_MS);
    CHECK(wifi_fx_gpio_power_on_writes() == 0);
    CHECK(wifi_fx_gpio_last_value() == 1);
    CHECK(params.wifi_enabled.x == false);
    CHECK(wifi_get_status() == WIFI_OFF);
    CHECK(wifi_fx_params_bool_set_calls() == 1);   /* solo el OFF del setup */
}

/* --- estado visible de NetworkManager -------------------------------------- */
static void test_nm_state_visible(void) {
    printf("\n== NetworkManager: conexion, reconexion y estado visible con el periodo nuevo ==\n");

    module_setup(true, true);
    CHECK(device != NULL);
    CHECK(wifi_get_status() == WIFI_DISCONNECTED);

    uint32_t msgs = wifi_fx_msg_state_changed();
    CHECK(msgs > 0);

    /* Conexion: NM avisa ACTIVATED -> el estado que ve el operador cambia a CONNECTED. */
    device_state_changed_sig_cb(device, NM_DEVICE_STATE_ACTIVATED, NM_DEVICE_STATE_CONFIG, 0, NULL);
    CHECK(wifi_get_status() == WIFI_CONNECTED);
    CHECK(wifi_fx_msg_state_changed() == msgs + 1);

    /* Reintento con clave mal: el mensaje que ya existia sigue saliendo. */
    device_state_changed_sig_cb(device, NM_DEVICE_STATE_FAILED, NM_DEVICE_STATE_CONFIG,
                                NM_DEVICE_STATE_REASON_NO_SECRETS, NULL);
    CHECK(strcmp(wifi_fx_last_msg_text(), "Wrong WiFi password") == 0);

    /* Desconexion: vuelve a DISCONNECTED y el bus se avisa. */
    device_state_changed_sig_cb(device, NM_DEVICE_STATE_DISCONNECTED, NM_DEVICE_STATE_ACTIVATED, 0, NULL);
    CHECK(wifi_get_status() == WIFI_DISCONNECTED);
    CHECK(wifi_fx_msg_state_changed() == msgs + 2);

    /* Todo eso paso con la radio encendida y sin tocar el timer: sigue uno solo, a 30 ms,
     * y la ventana ociosa mide el ritmo de la radio encendida. */
    CHECK(wifi_fx_timers_live() == 1);
    CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_ON_MS);
    CHECK(idle_wakeups(WINDOW_MS) == WINDOW_MS / PERIOD_ON_MS);
}

/* --- sin adaptador NM y contabilidad final --------------------------------- */
static void test_no_nm_adapter(void) {
    printf("\n== sin adaptador NM: el modulo arranca igual y no filtra timers ==\n");

    module_setup(false, false);
    CHECK(client == NULL);
    CHECK(device == NULL);

    /* El modulo ya hacia esto: sin cliente, g_signal_connect(NULL, ...) es un critical de
     * GLib (devuelve 0) y el LV_LOG_ERROR sale igual. No lo cambia #218. */
    CHECK(wifi_fx_signal_connect_null() == 1);
    CHECK(wifi_fx_log_calls(WIFI_FX_LOG_ERROR) == 1);
    CHECK(wifi_fx_log_calls(WIFI_FX_LOG_USER) == 1);   /* "Power off wifi/bt" */

    CHECK(loop_timer != NULL);
    CHECK(wifi_fx_timers_live() == 1);
    CHECK(wifi_fx_timer_period(loop_timer) == PERIOD_OFF_MS);
    CHECK(idle_wakeups(WINDOW_MS) == WINDOW_MS / PERIOD_OFF_MS);

    /* Contabilidad: lo creado es lo vivo mas lo borrado (nada quedo colgado). */
    CHECK(wifi_fx_timers_created() == wifi_fx_timers_live() + wifi_fx_timers_deleted());
    CHECK(wifi_fx_timers_live() == 1);
}

extern "C" void test_wifi_idle_timer(void) {
    /* Primero el instrumento (los numeros salen aunque el contrato falle), despues el
     * contrato. */
    measure_idle_wakeups();

    test_off_at_start();
    test_on_at_start();
    test_100_cycles();
    test_scan_pending_at_off();
    test_late_callback_after_off();
    test_nm_state_visible();
    test_no_nm_adapter();

    printf("\n== resumen de la medicion: OFF %u vueltas/%u ms (%.1f/s), ON %u vueltas/%u ms (%.1f/s) ==\n",
           g_off_wakeups, WINDOW_MS, (double)g_off_wakeups * 1000.0 / (double)WINDOW_MS, g_on_wakeups,
           WINDOW_MS, (double)g_on_wakeups * 1000.0 / (double)WINDOW_MS);
}

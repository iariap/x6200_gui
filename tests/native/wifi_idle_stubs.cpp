/*
 * Fixture implementation of the WiFi idle-pump native stage (issue #218, F-06).
 *
 * What this file is NOT: a mock of src/wifi.cpp. The module under test is the real
 * source (staged by tests/native/run_wifi_idle_timer.sh and included by
 * tests/native/test_wifi_idle_timer.cpp), so its timer creation, its period changes and
 * its state/callback wiring run exactly as they do on the radio. What is replaced is the
 * world around it:
 *
 *   - LVGL's timer engine: a 16-slot pool that mirrors lvgl/src/misc/lv_timer.c
 *     (`due when now - last_run >= period`, `last_run = now` before the callback,
 *     `set_period` only swaps the period) over a virtual clock. The pool keeps the record
 *     after lv_timer_del() so the deleted timer count is inspectable; nothing is recycled,
 *     so a double free or a stale pointer is loud instead of silent.
 *   - the GLib main loop: `g_main_context_iteration()` counts its calls (each one is a
 *     wakeup of the pump the module drives from its LVGL timer) and dispatches every
 *     pending source - exactly what the default context does. `g_main_loop_quit()` is
 *     counted and does NOT destroy the loop (on the radio nobody runs
 *     g_main_loop_run(), traffic is pumped from the LVGL callback).
 *   - libnm and the GPIO: two NetworkManager scenarios (no wlan0 device / one device) and
 *     a recorder for the WiFi pin. Enum values and setting keys are the host libnm's, so
 *     the module's switch/comparisons walk the same numbers as on the radio.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wifi_idle_fixture.h"
#include "msg.h"
#include "pubsub_ids.h"

/* =========================== timers (espejo de lv_timer.c) ================== */

#define WIFI_FX_TIMERS   16
#define WIFI_FX_SOURCES   8
#define WIFI_FX_SIGNALS   8

/* El struct que LVGL define en lv_timer.h (aca vive en el pool del fixture). */
struct _lv_timer_t {
    lv_timer_cb_t cb;
    void         *user_data;
    uint32_t      period;
    uint32_t      last_run;
    bool          live;
    bool          used;
};

static lv_timer_t fx_timers[WIFI_FX_TIMERS];

static uint32_t fx_clock;
static uint32_t fx_created;
static uint32_t fx_deleted;

/* Un timer invisible o ya borrado es un bug del modulo, no una medicion: se corta. */
static void fx_gone(const char *what, const void *timer) {
    fprintf(stderr, "fixture: %s sobre un timer inexistente (%p)\n", what, timer);
    exit(2);
}

extern "C" lv_timer_t *lv_timer_create(lv_timer_cb_t timer_xcb, uint32_t period, void *user_data) {
    for (int i = 0; i < WIFI_FX_TIMERS; i++) {
        if (fx_timers[i].used) {
            continue;
        }

        lv_timer_t *timer = &fx_timers[i];

        timer->cb = timer_xcb;
        timer->user_data = user_data;
        timer->period = period;
        timer->last_run = fx_clock;
        timer->live = true;
        timer->used = true;
        fx_created++;

        return timer;
    }

    fprintf(stderr, "fixture: el modulo creo mas de %d timers sin borrarlos\n", WIFI_FX_TIMERS);
    exit(2);
}

extern "C" void lv_timer_set_period(lv_timer_t *timer, uint32_t period) {
    if (timer == NULL || !timer->live) {
        fx_gone("lv_timer_set_period()", timer);
    }

    timer->period = period;
}

extern "C" void lv_timer_del(lv_timer_t *timer) {
    if (timer == NULL || !timer->live) {
        fx_gone("lv_timer_del()", timer);
    }

    timer->live = false;
    fx_deleted++;
}

extern "C" uint32_t wifi_fx_tick(void) {
    return fx_clock;
}

extern "C" uint32_t wifi_fx_timers_live(void) {
    uint32_t live = 0;

    for (int i = 0; i < WIFI_FX_TIMERS; i++) {
        if (fx_timers[i].used && fx_timers[i].live) {
            live++;
        }
    }

    return live;
}

extern "C" uint32_t wifi_fx_timers_created(void) {
    return fx_created;
}

extern "C" uint32_t wifi_fx_timers_deleted(void) {
    return fx_deleted;
}

extern "C" uint32_t wifi_fx_timer_period(const lv_timer_t *timer) {
    if (timer == NULL) {
        fx_gone("wifi_fx_timer_period()", timer);
    }

    return timer->period;
}

extern "C" void wifi_fx_align(lv_timer_t *timer) {
    if (timer == NULL || !timer->live) {
        fx_gone("wifi_fx_align()", timer);
    }

    uint32_t elapsed = fx_clock - timer->last_run;
    uint32_t due_in = (elapsed >= timer->period) ? 0 : timer->period - elapsed;

    if (due_in > 0) {
        wifi_fx_run(due_in);
    }
}

/* =========================== pump de GLib ================================= */

struct fx_source {
    void (*cb)(void *);
    void *user_data;
    bool  pending;
};

static struct fx_source fx_sources[WIFI_FX_SOURCES];
static uint32_t         fx_iterations;
static uint32_t         fx_quits;
static uint32_t         fx_dispatched;
static int              fx_loop_token;
static int              fx_context_token;

extern "C" uint32_t wifi_fx_run(uint32_t window_ms) {
    uint32_t end = fx_clock + window_ms;
    uint32_t before = fx_iterations;

    while (fx_clock < end) {
        fx_clock++;

        for (int i = 0; i < WIFI_FX_TIMERS; i++) {
            lv_timer_t *timer = &fx_timers[i];

            if (!timer->live) {
                continue;
            }

            /* lv_timer_exec(): vence cuando ya paso el periodo; last_run se mueve ANTES de
             * llamar al callback (el callback puede borrar el timer, como en LVGL). */
            if (fx_clock - timer->last_run >= timer->period) {
                timer->last_run = fx_clock;

                if (timer->cb) {
                    timer->cb(timer);
                }
            }
        }
    }

    return fx_iterations - before;
}

extern "C" uint32_t wifi_fx_pump_iterations(void) {
    return fx_iterations;
}

extern "C" uint32_t wifi_fx_pump_quits(void) {
    return fx_quits;
}

extern "C" uint32_t wifi_fx_async_pending(void) {
    uint32_t pending = 0;

    for (int i = 0; i < WIFI_FX_SOURCES; i++) {
        if (fx_sources[i].pending) {
            pending++;
        }
    }

    return pending;
}

extern "C" uint32_t wifi_fx_async_dispatched(void) {
    return fx_dispatched;
}

extern "C" void wifi_fx_post(void (*cb)(void *), void *user_data) {
    for (int i = 0; i < WIFI_FX_SOURCES; i++) {
        if (!fx_sources[i].pending && fx_sources[i].cb == NULL) {
            fx_sources[i].cb = cb;
            fx_sources[i].user_data = user_data;
            fx_sources[i].pending = true;
            return;
        }
    }

    fprintf(stderr, "fixture: mas de %d fuentes async pendientes\n", WIFI_FX_SOURCES);
    exit(2);
}

extern "C" GMainLoop *g_main_loop_new(GMainContext *context, gboolean is_running) {
    (void)context;
    (void)is_running;

    /* Token opaco: el modulo nunca desreferencia el loop ni el contexto (solo los pasa
     * entre g_main_loop_get_context() y g_main_context_iteration()). */
    return (GMainLoop *)&fx_loop_token;
}

extern "C" GMainContext *g_main_loop_get_context(GMainLoop *loop) {
    (void)loop;

    return (GMainContext *)&fx_context_token;
}

extern "C" void g_main_loop_quit(GMainLoop *loop) {
    (void)loop;

    /* GLib: sobre un loop que no esta corriendo solo baja su bandera; no lo destruye. */
    fx_quits++;
}

extern "C" gboolean g_main_context_iteration(GMainContext *context, gboolean may_block) {
    (void)context;
    (void)may_block;

    fx_iterations++;

    gboolean dispatched = FALSE;

    for (int i = 0; i < WIFI_FX_SOURCES; i++) {
        if (!fx_sources[i].pending) {
            continue;
        }

        void (*cb)(void *) = fx_sources[i].cb;
        void *user_data = fx_sources[i].user_data;

        fx_sources[i].pending = false;
        fx_sources[i].cb = NULL;
        fx_sources[i].user_data = NULL;
        fx_dispatched++;
        dispatched = TRUE;

        cb(user_data);
    }

    return dispatched;
}

/* =========================== GObject / bytes ============================== */

struct _GBytes {
    unsigned char *data;
    gsize          len;
};

struct fx_signal {
    char     name[32];
    uint32_t count;
};

static struct fx_signal fx_signals[WIFI_FX_SIGNALS];
static uint32_t         fx_signal_null;

extern "C" void g_object_set(gpointer object, const gchar *first_property_name, ...) {
    /* libnm primitive, no logica del modulo: las propiedades de los settings no cambian el
     * timer ni el pump, y caminar la lista de varargs seria fingir libnm. */
    (void)object;
    (void)first_property_name;
}

extern "C" void g_object_unref(gpointer object) {
    (void)object;
}

extern "C" gulong g_signal_connect(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
                                   gpointer data) {
    (void)c_handler;
    (void)data;

    /* GLib real: g_return_val_if_fail(G_TYPE_CHECK_INSTANCE(instance), 0) -> un critical y
     * devuelve 0 cuando la instancia es NULL. No aborta, y el modulo (preexistente) sigue
     * sin chequear el cliente: aca se cuenta para poder declararlo, no se "arregla". */
    if (instance == NULL) {
        fx_signal_null++;
        return 0;
    }

    for (int i = 0; i < WIFI_FX_SIGNALS; i++) {
        if (fx_signals[i].name[0] == '\0') {
            snprintf(fx_signals[i].name, sizeof(fx_signals[i].name), "%s", detailed_signal);
        }

        if (strcmp(fx_signals[i].name, detailed_signal) == 0) {
            fx_signals[i].count++;
            return (gulong)(i + 1);
        }
    }

    return 0;
}

extern "C" uint32_t wifi_fx_signal_connects(const char *signal_name) {
    for (int i = 0; i < WIFI_FX_SIGNALS; i++) {
        if (strcmp(fx_signals[i].name, signal_name) == 0) {
            return fx_signals[i].count;
        }
    }

    return 0;
}

extern "C" uint32_t wifi_fx_signal_connect_null(void) {
    return fx_signal_null;
}

extern "C" GBytes *g_bytes_new(gconstpointer data, gsize size) {
    GBytes *bytes = (GBytes *)calloc(1, sizeof(GBytes));

    if (bytes == NULL) {
        exit(2);
    }

    bytes->data = (unsigned char *)calloc(1, size ? size : 1);
    if (bytes->data == NULL) {
        exit(2);
    }

    if (data != NULL && size > 0) {
        memcpy(bytes->data, data, size);
    }
    bytes->len = size;

    return bytes;
}

extern "C" gconstpointer g_bytes_get_data(GBytes *bytes, gsize *size) {
    if (bytes == NULL) {
        return NULL;
    }

    if (size != NULL) {
        *size = bytes->len;
    }

    return bytes->data;
}

extern "C" gsize g_bytes_get_size(GBytes *bytes) {
    return bytes == NULL ? 0 : bytes->len;
}

extern "C" gint g_bytes_compare(GBytes *bytes1, GBytes *bytes2) {
    if (bytes1 == NULL || bytes2 == NULL) {
        return bytes1 == bytes2 ? 0 : (bytes1 == NULL ? -1 : 1);
    }

    gsize min = bytes1->len < bytes2->len ? bytes1->len : bytes2->len;
    int   cmp = min > 0 ? memcmp(bytes1->data, bytes2->data, min) : 0;

    if (cmp != 0) {
        return cmp;
    }

    return bytes1->len == bytes2->len ? 0 : (bytes1->len < bytes2->len ? -1 : 1);
}

extern "C" gchar *g_strdup(const gchar *str) {
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str) + 1;
    gchar *copy = (gchar *)malloc(len);

    if (copy == NULL) {
        exit(2);
    }

    memcpy(copy, str, len);

    return copy;
}

extern "C" void g_free(gpointer mem) {
    free(mem);
}

extern "C" void g_error_free(GError *error) {
    /* El fixture devuelve un GError estatico (ver wifi_fx_error_no_nm): liberarlo con
     * free() seria peor que no hacer nada. */
    (void)error;
}

/* =========================== libnm ======================================== */

static int  fx_nm_client_token;
static int  fx_nm_device_token;
static bool fx_nm_present;
static uint64_t fx_nm_last_scan = 1000;

/* El GError que el modulo lee cuando no hay NetworkManager (issue #218: "sin adaptador
 * NM"): tiene que existir y traer mensaje, o el LV_LOG_ERROR del modulo desreferencia NULL. */
static GError fx_error_no_nm = { 0, 1, (gchar *)"fixture: NetworkManager no disponible" };

struct fx_async {
    GAsyncReadyCallback cb;
    gpointer            user_data;
    GObject            *source;
    bool                armed;
};

static struct fx_async fx_scan;
static int              fx_async_result_token;

extern "C" void wifi_fx_nm_present(bool present) {
    fx_nm_present = present;
}

extern "C" void wifi_fx_nm_last_scan(uint64_t value) {
    fx_nm_last_scan = value;
}

extern "C" NMDevice *wifi_fx_nm_device(void) {
    return fx_nm_present ? (NMDevice *)&fx_nm_device_token : NULL;
}

static void fx_invoke_scan(void *user_data) {
    (void)user_data;

    if (!fx_scan.armed) {
        return;
    }

    fx_scan.armed = false;
    fx_scan.cb(fx_scan.source, (GAsyncResult *)&fx_async_result_token, fx_scan.user_data);
}

extern "C" NMClient *nm_client_new(GCancellable *cancellable, GError **error) {
    (void)cancellable;

    if (!fx_nm_present) {
        if (error != NULL) {
            *error = &fx_error_no_nm;
        }

        return NULL;
    }

    if (error != NULL) {
        *error = NULL;
    }

    return (NMClient *)&fx_nm_client_token;
}

extern "C" NMDevice *nm_client_get_device_by_iface(NMClient *client, const char *iface) {
    (void)client;

    return (strcmp(iface, "wlan0") == 0) ? wifi_fx_nm_device() : NULL;
}

extern "C" NMRemoteConnection *nm_client_get_connection_by_id(NMClient *client, const char *id) {
    (void)client;
    (void)id;

    /* Sin conexiones guardadas: la GUI muestra la lista vacia y no hay nada que activar. */
    return NULL;
}

extern "C" const char *nm_device_get_iface(NMDevice *device) {
    (void)device;

    return "wlan0";
}

extern "C" guint nm_device_get_state(NMDevice *device) {
    (void)device;

    return NM_DEVICE_STATE_DISCONNECTED;
}

extern "C" NMActiveConnection *nm_device_get_active_connection(NMDevice *device) {
    (void)device;

    return NULL;
}

extern "C" NMIPConfig *nm_device_get_ip4_config(NMDevice *device) {
    (void)device;

    return NULL;
}

extern "C" void nm_device_disconnect_async(NMDevice *device, GCancellable *cancellable,
                                           GAsyncReadyCallback callback, gpointer user_data) {
    (void)device;
    (void)cancellable;
    (void)callback;
    (void)user_data;
}

extern "C" gboolean nm_device_disconnect_finish(NMDevice *device, GAsyncResult *result, GError **error) {
    (void)device;
    (void)result;

    if (error != NULL) {
        *error = NULL;
    }

    return TRUE;
}

extern "C" gint64 nm_device_wifi_get_last_scan(NMDeviceWifi *device) {
    (void)device;

    return (gint64)fx_nm_last_scan;
}

extern "C" void nm_device_wifi_request_scan_async(NMDeviceWifi *device, GCancellable *cancellable,
                                                  GAsyncReadyCallback callback, gpointer user_data) {
    (void)cancellable;

    /* libnm contesta por la fuente async del contexto, no en la misma vuelta. */
    fx_scan.cb = callback;
    fx_scan.user_data = user_data;
    fx_scan.source = (GObject *)device;
    fx_scan.armed = true;

    wifi_fx_post(fx_invoke_scan, NULL);
}

extern "C" gboolean nm_device_wifi_request_scan_finish(NMDeviceWifi *device, GAsyncResult *result,
                                                       GError **error) {
    (void)device;
    (void)result;

    if (error != NULL) {
        *error = NULL;
    }

    return TRUE;
}

extern "C" const GPtrArray *nm_device_wifi_get_access_points(NMDeviceWifi *device) {
    (void)device;

    return NULL;
}

extern "C" NMAccessPoint *nm_device_wifi_get_active_access_point(NMDeviceWifi *device) {
    (void)device;

    return NULL;
}

/* --- libnm que este stage no ejercita -------------------------------------- */
/* El modulo referencia estas funciones, asi que el binario necesita sus definiciones; el
 * escenario de agregar/editar/borrar conexiones y el de leer la IP no forman parte de lo
 * que mide #218 (el pump ocioso). */

extern "C" GBytes *nm_access_point_get_ssid(NMAccessPoint *ap) {
    (void)ap;

    return NULL;
}

extern "C" NM80211ApFlags nm_access_point_get_flags(NMAccessPoint *ap) {
    (void)ap;

    return NM_802_11_AP_FLAGS_NONE;
}

extern "C" guint32 nm_access_point_get_wpa_flags(NMAccessPoint *ap) {
    (void)ap;

    return 0;
}

extern "C" guint32 nm_access_point_get_rsn_flags(NMAccessPoint *ap) {
    (void)ap;

    return 0;
}

extern "C" const char *nm_access_point_get_bssid(NMAccessPoint *ap) {
    (void)ap;

    return "00:00:00:00:00:00";
}

extern "C" NM80211Mode nm_access_point_get_mode(NMAccessPoint *ap) {
    (void)ap;

    return NM_802_11_MODE_INFRA;
}

extern "C" guint8 nm_access_point_get_strength(NMAccessPoint *ap) {
    (void)ap;

    return 0;
}

extern "C" NMConnection *nm_simple_connection_new(void) {
    return NULL;
}

extern "C" NMConnection *nm_simple_connection_new_clone(NMConnection *connection) {
    (void)connection;

    return NULL;
}

extern "C" NMSetting *nm_setting_connection_new(void) {
    return NULL;
}

extern "C" NMSetting *nm_setting_wireless_new(void) {
    return NULL;
}

extern "C" NMSetting *nm_setting_wireless_security_new(void) {
    return NULL;
}

extern "C" NMSetting *nm_setting_ip4_config_new(void) {
    return NULL;
}

extern "C" NMSetting *nm_setting_ip6_config_new(void) {
    return NULL;
}

extern "C" GType nm_setting_wireless_security_get_type(void) {
    return 1;
}

extern "C" void nm_connection_add_setting(NMConnection *connection, NMSetting *setting) {
    (void)connection;
    (void)setting;
}

extern "C" void nm_connection_remove_setting(NMConnection *connection, GType setting_type) {
    (void)connection;
    (void)setting_type;
}

extern "C" void nm_connection_replace_settings_from_connection(NMConnection *connection,
                                                              NMConnection *new_connection) {
    (void)connection;
    (void)new_connection;
}

extern "C" const char *nm_connection_get_path(NMConnection *connection) {
    (void)connection;

    return "/";
}

extern "C" const char *nm_connection_get_id(NMConnection *connection) {
    (void)connection;

    return "";
}

extern "C" const char *nm_connection_get_uuid(NMConnection *connection) {
    (void)connection;

    return "";
}

extern "C" const char *nm_active_connection_get_id(NMActiveConnection *connection) {
    (void)connection;

    return "";
}

extern "C" void nm_client_add_connection_async(NMClient *client, NMConnection *connection, gboolean save_to_disk,
                                               GCancellable *cancellable, GAsyncReadyCallback callback,
                                               gpointer user_data) {
    (void)client;
    (void)connection;
    (void)save_to_disk;
    (void)cancellable;
    (void)callback;
    (void)user_data;
}

extern "C" NMRemoteConnection *nm_client_add_connection_finish(NMClient *client, GAsyncResult *result,
                                                               GError **error) {
    (void)client;
    (void)result;
    (void)error;

    return NULL;
}

extern "C" void nm_client_add_and_activate_connection_async(NMClient *client, NMConnection *partial,
                                                            NMDevice *device, const char *specific_object,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback, gpointer user_data) {
    (void)client;
    (void)partial;
    (void)device;
    (void)specific_object;
    (void)cancellable;
    (void)callback;
    (void)user_data;
}

extern "C" NMActiveConnection *nm_client_add_and_activate_connection_finish(NMClient *client, GAsyncResult *result,
                                                                           GError **error) {
    (void)client;
    (void)result;
    (void)error;

    return NULL;
}

extern "C" void nm_client_activate_connection_async(NMClient *client, NMConnection *connection, NMDevice *device,
                                                    const char *specific_object, GCancellable *cancellable,
                                                    GAsyncReadyCallback callback, gpointer user_data) {
    (void)client;
    (void)connection;
    (void)device;
    (void)specific_object;
    (void)cancellable;
    (void)callback;
    (void)user_data;
}

extern "C" NMActiveConnection *nm_client_activate_connection_finish(NMClient *client, GAsyncResult *result,
                                                                    GError **error) {
    (void)client;
    (void)result;
    (void)error;

    return NULL;
}

extern "C" void nm_remote_connection_commit_changes_async(NMRemoteConnection *connection, gboolean save_to_disk,
                                                          GCancellable *cancellable,
                                                          GAsyncReadyCallback callback, gpointer user_data) {
    (void)connection;
    (void)save_to_disk;
    (void)cancellable;
    (void)callback;
    (void)user_data;
}

extern "C" gboolean nm_remote_connection_commit_changes_finish(NMRemoteConnection *connection,
                                                               GAsyncResult *result, GError **error) {
    (void)connection;
    (void)result;
    (void)error;

    return TRUE;
}

extern "C" void nm_remote_connection_delete_async(NMRemoteConnection *connection, GCancellable *cancellable,
                                                  GAsyncReadyCallback callback, gpointer user_data) {
    (void)connection;
    (void)cancellable;
    (void)callback;
    (void)user_data;
}

extern "C" gboolean nm_remote_connection_delete_finish(NMRemoteConnection *connection, GAsyncResult *result,
                                                       GError **error) {
    (void)connection;
    (void)result;
    (void)error;

    return TRUE;
}

extern "C" const char *nm_ip_config_get_gateway(NMIPConfig *config) {
    (void)config;

    return NULL;
}

extern "C" GPtrArray *nm_ip_config_get_addresses(NMIPConfig *config) {
    (void)config;

    return NULL;
}

extern "C" const char *nm_ip_address_get_address(NMIPAddress *address) {
    (void)address;

    return "";
}

extern "C" char *nm_utils_uuid_generate(void) {
    return g_strdup("00000000-0000-0000-0000-000000000000");
}

extern "C" char *nm_utils_ssid_to_utf8(const guint8 *ssid, gsize len) {
    (void)ssid;
    (void)len;

    return NULL;
}

extern "C" gboolean nm_utils_wpa_psk_valid(const char *psk) {
    return psk != NULL;
}

/* =========================== GPIO / params / mensajes ===================== */

int x6200_pin_wifi = 100;   /* el numero real lo define el SDK; aca solo tiene que ser un pin. */

static int      fx_gpio_last_pin = -1;
static int      fx_gpio_last_value = -1;
static uint32_t fx_gpio_writes;
static uint32_t fx_gpio_power_on_writes;

struct wifi_fx_params params;

static uint32_t fx_params_set_calls;
static uint32_t fx_log_calls[WIFI_FX_LOG_LEVELS];
static uint32_t fx_msg_sent;
static uint32_t fx_msg_state_changed;
static char     fx_last_msg_text[128] = "";

extern "C" void gpio_safe_set(int pin, int value) {
    fx_gpio_last_pin = pin;
    fx_gpio_last_value = value;
    fx_gpio_writes++;

    /* Nivel 0 = radio encendida (src/wifi.cpp: wifi_power_on()). */
    if (pin == x6200_pin_wifi && value == 0) {
        fx_gpio_power_on_writes++;
    }
}

extern "C" int wifi_fx_gpio_last_pin(void) {
    return fx_gpio_last_pin;
}

extern "C" int wifi_fx_gpio_last_value(void) {
    return fx_gpio_last_value;
}

extern "C" uint32_t wifi_fx_gpio_writes(void) {
    return fx_gpio_writes;
}

extern "C" uint32_t wifi_fx_gpio_power_on_writes(void) {
    return fx_gpio_power_on_writes;
}

extern "C" void params_bool_set(params_bool_t *var, bool x) {
    var->x = x;
    var->dirty = true;
    fx_params_set_calls++;
}

extern "C" uint32_t wifi_fx_params_bool_set_calls(void) {
    return fx_params_set_calls;
}

extern "C" void wifi_fx_log_call(int level) {
    if (level >= 0 && level < WIFI_FX_LOG_LEVELS) {
        fx_log_calls[level]++;
    }
}

extern "C" uint32_t wifi_fx_log_calls(int level) {
    if (level < 0 || level >= WIFI_FX_LOG_LEVELS) {
        return 0;
    }

    return fx_log_calls[level];
}

extern "C" void lv_msg_send(lv_msg_id_t msg_id, const void *payload) {
    (void)payload;

    fx_msg_sent++;

    if (msg_id == (lv_msg_id_t)MSG_WIFI_STATE_CHANGED) {
        fx_msg_state_changed++;
    }
}

extern "C" uint32_t wifi_fx_msg_sent(void) {
    return fx_msg_sent;
}

extern "C" uint32_t wifi_fx_msg_state_changed(void) {
    return fx_msg_state_changed;
}

extern "C" void msg_update_text_fmt(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vsnprintf(fx_last_msg_text, sizeof(fx_last_msg_text), fmt, args);
    va_end(args);
}

extern "C" const char *wifi_fx_last_msg_text(void) {
    return fx_last_msg_text;
}

/* =========================== reset ======================================== */

extern "C" void wifi_fx_reset(void) {
    for (int i = 0; i < WIFI_FX_TIMERS; i++) {
        memset(&fx_timers[i], 0, sizeof(fx_timers[i]));
    }
    for (int i = 0; i < WIFI_FX_SOURCES; i++) {
        memset(&fx_sources[i], 0, sizeof(fx_sources[i]));
    }
    for (int i = 0; i < WIFI_FX_SIGNALS; i++) {
        memset(&fx_signals[i], 0, sizeof(fx_signals[i]));
    }

    fx_clock = 0;
    fx_created = 0;
    fx_deleted = 0;
    fx_iterations = 0;
    fx_quits = 0;
    fx_dispatched = 0;
    fx_signal_null = 0;
    fx_nm_present = false;
    fx_nm_last_scan = 1000;
    fx_scan.cb = NULL;
    fx_scan.user_data = NULL;
    fx_scan.source = NULL;
    fx_scan.armed = false;
    fx_gpio_last_pin = -1;
    fx_gpio_last_value = -1;
    fx_gpio_writes = 0;
    fx_gpio_power_on_writes = 0;
    fx_params_set_calls = 0;
    fx_msg_sent = 0;
    fx_msg_state_changed = 0;
    fx_last_msg_text[0] = '\0';
    for (int i = 0; i < WIFI_FX_LOG_LEVELS; i++) {
        fx_log_calls[i] = 0;
    }

    memset(&params, 0, sizeof(params));
}

/*
 * Minimal LVGL subset for the WiFi idle-pump native stage (issue #218, F-06).
 *
 * Only what src/wifi.cpp uses: the timer API, the message bus and the log macros.
 * The signatures are the ones of LVGL 8.3.11, the subtree the ROM pins (lv_timer.h,
 * extra/others/msg/lv_msg.h).
 *
 * The engine behind lv_timer_create()/lv_timer_set_period()/lv_timer_del() is not the
 * real one: it is the small fixture of tests/native/wifi_idle_stubs.cpp, which mirrors
 * the scheduling of lvgl/src/misc/lv_timer.c (a timer is due when
 * `now - last_run >= period`, and firing it sets `last_run = now` and then calls the
 * callback; set_period only changes `period`). The real engine cannot be linked here:
 * the CI job of the native suite checks out without the lvgl/ submodule, so
 * `run_native_tests.sh --ci-parity` runs every stage with lvgl/ hidden.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
/* LVGL 8.3.11 llega a <string.h> por sus propios headers (src/misc/lv_mem.h lo incluye) y
 * varios modulos de la GUI -wifi.cpp, que usa strlen/strcpy/strcmp- dependen de ese include
 * transitivo. El stub lo reproduce, o el stage se rompe por un include que en la radio existe. */
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lv_timer_t lv_timer_t;
typedef void (*lv_timer_cb_t)(lv_timer_t *timer);
typedef uint32_t lv_msg_id_t;

lv_timer_t *lv_timer_create(lv_timer_cb_t timer_xcb, uint32_t period, void *user_data);
void        lv_timer_set_period(lv_timer_t *timer, uint32_t period);
void        lv_timer_del(lv_timer_t *timer);

/* lv_msg_send() (LVGL msg bus): el fixture cuenta los envios y guarda el id. */
void lv_msg_send(lv_msg_id_t msg_id, const void *payload);

/* Los niveles de LV_LOG_* que el modulo usa. El fixture los cuenta, no los imprime:
 * los formatos del modulo son los de LVGL (donde ya se validan con el atributo de
 * formato) y varios de ellos usan %zu con argumentos `guint`, asi que reproducirlos
 * aca mostraria valores que no son los de la radio. Lo que si se verifica -y es lo
 * que el operador ve- son los mensajes de msg_update_text_fmt() y los envios del bus. */
enum {
    WIFI_FX_LOG_INFO = 0,
    WIFI_FX_LOG_WARN,
    WIFI_FX_LOG_ERROR,
    WIFI_FX_LOG_USER,
    WIFI_FX_LOG_LEVELS
};

void wifi_fx_log_call(int level);

#define LV_LOG_INFO(fmt, ...)  wifi_fx_log_call(WIFI_FX_LOG_INFO)
#define LV_LOG_WARN(fmt, ...)  wifi_fx_log_call(WIFI_FX_LOG_WARN)
#define LV_LOG_ERROR(fmt, ...) wifi_fx_log_call(WIFI_FX_LOG_ERROR)
#define LV_LOG_USER(fmt, ...)  wifi_fx_log_call(WIFI_FX_LOG_USER)

#ifdef __cplusplus
}
#endif

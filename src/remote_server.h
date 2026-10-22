/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Web remote: local control socket for the webserver (/remote page)
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

void remote_server_init();

/* A web client or X6100 Link follows the state (any thread) */
bool remote_subscribed();

/* An event ({"t":...}) for the subscribed web clients. Any thread; takes o and frees it. */
void remote_event(cJSON *o);

/* Decoded text of type ("rtty_text"...) for the subscribed web clients, batched into
 * {"t":type,"text":...} at a newline, 300 ms or 200 characters. Any thread; nothing without subscribers. */
void remote_text(const char *type, const char *s, size_t n);

/* Band by metres (40, 20...): the last of its sub-bands used, else the lowest, at its last
 * frequency and mode. LVGL thread. Also the mic number keys (main_screen.c). */
void remote_set_band(int32_t m);

/* Waterfall PSD (dB) from the DSP, sent to the subscribed web clients (~10 fps) */
void remote_scope_data(const float *psd_db, size_t n, uint32_t center_hz, uint32_t width_hz, bool tx);

#ifdef __cplusplus
}
#endif

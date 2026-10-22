/*
 * Minimal remote_server/cJSON stub for the native CW keyboard dialog test (#217).
 *
 * src/dialog_cw_kbd.c builds a {"t":"cw_tx","text":...} event and hands it to
 * remote_event(). The stub counts the events instead of reaching the webserver.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef struct cJSON cJSON;

cJSON *cJSON_CreateObject(void);
void   cJSON_AddStringToObject(cJSON *o, const char *name, const char *s);

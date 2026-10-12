/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - live remote control (not ported yet).
 *
 *  The X6100 apps publish their state to the web Remote page through this
 *  socket. Until the remote server is ported these are no-ops, so the ported
 *  app dialogs work unchanged and the Remote page shows the radio offline.
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The X6100 apps call this with a JSON object; nothing to publish yet */
#define remote_event(...) ((void) 0)

static inline int remote_state(char *buf, size_t size) {
    (void) buf;
    (void) size;

    return 0;
}

#ifdef __cplusplus
}
#endif

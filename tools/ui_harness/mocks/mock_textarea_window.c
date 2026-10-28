/*
 * Host mock of src/textarea_window.c for the UI harness.
 *
 * Ajustes (src/dialog_settings.c) uses the textarea window for the Logs upload
 * credentials rows. The harness fixtures never type in those rows; they only
 * need the symbols to link, so the window is inert and keeps a tiny buffer.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <string.h>

#include "textarea_window.h"

/* ui_harness_dialogs already ships these mocks in mocks/dialogs/mock_dialog_deps.c */
#ifndef HARNESS_REAL_DIALOGS

static char mock_text[256];

lv_obj_t *textarea_window_open(textarea_window_cb_t ok_cb, textarea_window_cb_t cancel_cb) {
    (void) ok_cb;
    (void) cancel_cb;

    return NULL;
}

lv_obj_t *textarea_window_open_w_label(textarea_window_cb_t ok_cb, textarea_window_cb_t cancel_cb,
                                       const char *text) {
    (void) text;

    return textarea_window_open(ok_cb, cancel_cb);
}

void textarea_window_close(void) {
}

const char *textarea_window_get(void) {
    return mock_text;
}

void textarea_window_set(const char *text) {
    if (text == NULL) {
        mock_text[0] = '\0';
        return;
    }

    strncpy(mock_text, text, sizeof(mock_text) - 1);
    mock_text[sizeof(mock_text) - 1] = '\0';
}

lv_obj_t *textarea_window_text(void) {
    return NULL;
}
#endif

/*
 * Native test for the app radio-state restore (issue #94: "el barrido de apps
 * deja el modo y el dial cambiados").
 *
 * The defect: an app that changes the radio on open (FT8/WSPR/JS8 fix their dial
 * and mode) must put it back when it closes. The restore used to depend on the
 * SQLite-backed cfg_memory_save/load(MEM_BACKUP_ID) slot, called per dialog from
 * dialog_construct/dialog_destruct, from FT8's own dtor and from the CW remote
 * path; the fix moves the snapshot to the in-memory radio_state_save/
 * radio_state_restore (src/radio.c, the same pattern as radio_start_swrscan,
 * #84/#96) so the frame is the single place that saves on open and restores on
 * close.
 *
 * The REAL src/dialog.c is staged next to minimal stubs
 * (tests/native/stubs_restore/), the same pattern as the clock/backlight/
 * digital-modes/SWR-Scan tests. The stubs carry a real radio_state_save/restore
 * (the snapshot radio.c keeps) and a no-op cfg_memory_save/load: with no
 * database the old path cannot restore anything, so the test is red before the
 * fix and green after it.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>

#include "test_framework.h"

#include "dialog.h"
#include "cfg/subjects.h"

/* The radio state the owner had before the app opened */
#define OWNER_FREQ  7162660
#define OWNER_MODE  2   /* USB */
/* What the app sets when it opens (FT8's 40 m dial, USB-D) */
#define APP_FREQ    7074000
#define APP_MODE    3   /* USB-D */

static void app_construct_cb(lv_obj_t *parent) {
    (void) parent;

    /* lo que hace una app digital al abrir: fija su dial y su modo */
    subject_set_int(cfg_cur.fg_freq, APP_FREQ);
    subject_set_int(cfg_cur.mode, APP_MODE);
}

static void app_destruct_cb(void) {
}

void test_dialog_restore(void) {
    cfg_cur.fg_freq = subject_create_int(OWNER_FREQ);
    cfg_cur.mode    = subject_create_int(OWNER_MODE);
    cfg_cur.agc     = subject_create_int(1);
    cfg_cur.pre     = subject_create_int(0);
    cfg_cur.att     = subject_create_int(0);
    cfg.pwr.val     = subject_create_int(50);

    static dialog_t dialog = {
        .construct_cb  = app_construct_cb,
        .destruct_cb   = app_destruct_cb,
        .restore_radio = true,
    };

    dialog_construct(&dialog, NULL);
    CHECK(subject_get_int(cfg_cur.fg_freq) == APP_FREQ);   /* la app cambio el dial */
    CHECK(subject_get_int(cfg_cur.mode) == APP_MODE);      /* ... y el modo */

    dialog_destruct();
    CHECK(subject_get_int(cfg_cur.fg_freq) == OWNER_FREQ); /* <- falla antes del fix */
    CHECK(subject_get_int(cfg_cur.mode) == OWNER_MODE);

    /* El snapshot se consume: sin un save previo no hay nada que restaurar. */
    dialog_destruct();
    CHECK(subject_get_int(cfg_cur.fg_freq) == OWNER_FREQ);
}

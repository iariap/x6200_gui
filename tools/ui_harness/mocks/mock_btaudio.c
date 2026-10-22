/*
 *  Host mock for the Bluetooth app's script surface (UI-005).
 *
 *  `src/dialog_bt.c` shows what `btaudio-ctl` reports: it runs the script on a
 *  worker thread (`system("<args>; btaudio-ctl list > /tmp/bt_ui_list.txt")`) and
 *  a timer reads the file back. The host has no `btaudio-ctl` and must not run
 *  anything, so:
 *
 *  * `__wrap_system()` (linked with `-Wl,--wrap=system`) is a documented no-op:
 *    the script never runs, no thread runs a shell.
 *  * `mock_btaudio_seed_files()` writes the same file the script would have left
 *    (src/dialog_bt.c's BT_LIST_FILE) with a fixed, documented device list, so
 *    the dialog renders real rows and its real status line instead of an empty
 *    table. It is called by the `bt-dialog` screen builder (src/screens.c) before
 *    the dialog is constructed, so the content the timer reads is deterministic.
 *
 *  The other `btaudio_*` surface (log, run, speaker mute, headphone volume) is in
 *  mocks/mock_ui_deps.c.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

#include "mock_harness.h"

/* What `btaudio-ctl list` would print for one paired headphones device and no
 * active route (the tab-separated format src/bt_devices.c parses): name, role
 * and state the dialog's table draws. The values never change, so the render is
 * byte-identical on every run. */
static const char BT_LIST[] =
    "DEV\t2C:1B:3A:0A:64:BE\t0\thp\t0\tLiving Room TV\n"
    "HP\t0\t-1\n"
    "RX\toff\n"
    "MUTE\toff\n";

void mock_btaudio_seed_files(void)
{
    FILE *f = fopen("/tmp/bt_ui_list.txt", "w");

    if (f == NULL) {
        return;
    }
    fputs(BT_LIST, f);
    fclose(f);
}

int __wrap_system(const char *cmd)
{
    /* The real script is never run on the host: the state the dialog reads is
     * seeded by mock_btaudio_seed_files() instead. */
    (void) cmd;

    return 0;
}
